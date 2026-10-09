#include "graphics/TextureCache.hpp"
#include "graph_vram.h"
#include "graphics/VramHeap.hpp"
#include "logging/log.hpp"

#include <dma.h>
#include <gs_psm.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace graphics
{
namespace
{
using SmartPacket = std::unique_ptr<packet2_t, decltype(&packet2_free)>;
/// GS privileged register CSR, and its FINISH event bit. Writing 1 to the bit
/// clears it; it reads back as 1 once a FINISH register write has been processed.
volatile u64 *const GS_CSR = reinterpret_cast<volatile u64 *>(0x12001000);
constexpr u64 CSR_FINISH = u64(1) << 1;

/// Gives up spinning on a fence that never arrives rather than hanging the console
constexpr u32 MAX_FENCE_SPINS = 50000000u;

/// Fallback FINISH emitter: a FINISH register write through PATH3. This only
/// orders against other PATH3 traffic. Replace it via SetFenceEmitter with one
/// that goes through PATH1 behind VU1's primitives.
void EmitFinishThroughPath3()
{
    SmartPacket packet{packet2_create(2, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0),
                                                      &packet2_free};
    qword_t qword;

    qword.dw[0] = u64(GIF_SET_TAG(1, 1, 0, 0, GIF_FLG_PACKED, 1));
    qword.dw[1] = u64(GIF_REG_AD);
    packet2_add_u128(packet.get(), qword.qw);

    qword.dw[0] = 0;
    qword.dw[1] = GS_REG_FINISH;
    packet2_add_u128(packet.get(), qword.qw);

    dma_channel_send_packet2(packet.get(), DMA_CHANNEL_GIF, 0);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
}
} // namespace

TextureCache::TextureCache(u32 baseWords, u32 sizeWords)
    : vramHeap(baseWords, sizeWords), currentPass(0), fencePending(false), fenceHalf(0),
      fenceEmitter(&EmitFinishThroughPath3), stats()
{
    halfLoaded[0] = -1;
    halfLoaded[1] = -1;

    LOG_INFO("Texture cache base=") << vramHeap.GetBase() << " words, 2 halves of " << vramHeap.GetHalfCapacity()
                                    << " words (" << (vramHeap.GetHalfCapacity() * 4 / 1024) << " KB) each";
}

TextureCache::TextureCache() : TextureCache(ProbeVramBase(), GRAPH_VRAM_MAX_WORDS - ProbeVramBase())
{
}

TextureCache::~TextureCache()
{
    for (size_t p = 0; p < passes.size(); ++p)
    {
        for (size_t t = 0; t < passes[p].textures.size(); ++t)
        {
            passes[p].textures[t]->OnEvicted();
            passes[p].textures[t]->SetResidencyOwner(nullptr);
        }
    }
}

u32 TextureCache::ProbeVramBase()
{
    const int base = graph_vram_allocate(0, 0, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    return base > 0 ? static_cast<u32>(base) : 0u;
}

void TextureCache::SetFenceEmitter(FenceEmitter emitter)
{
    fenceEmitter = emitter != nullptr ? emitter : &EmitFinishThroughPath3;
}

void TextureCache::EmitFence()
{
    *GS_CSR = CSR_FINISH; // clear the FINISH bit first
    fenceEmitter();       // then queue FINISH after the last primitive to wait on
}

void TextureCache::WaitFence()
{
    if (!fencePending)
        return;

    u32 spins = 0;
    while ((*GS_CSR & CSR_FINISH) == 0)
    {
        if (++spins >= MAX_FENCE_SPINS)
        {
            LOG_ERROR("TextureCache: gave up waiting for GS FINISH, the fence emitter may not reach the GS");
            break;
        }
    }
    *GS_CSR |= CSR_FINISH;

    // Zero spins means the fence was already signalled: the stall was hidden
    // behind the pass. Anything else is time lost on the critical path.
    if (spins > 0)
    {
        ++stats.fenceStalls;
        stats.fenceSpins += spins;
    }
    fencePending = false;
}

int TextureCache::GetPassOf(const Texture &tex) const
{
    for (size_t p = 0; p < passes.size(); ++p)
    {
        const std::vector<Texture *> &list = passes[p].textures;
        if (std::find(list.begin(), list.end(), &tex) != list.end())
            return static_cast<int>(p);
    }
    return -1;
}

void TextureCache::ClearPlan()
{
    WaitFence();

    for (size_t p = 0; p < passes.size(); ++p)
    {
        for (size_t t = 0; t < passes[p].textures.size(); ++t)
        {
            passes[p].textures[t]->OnEvicted();
            passes[p].textures[t]->SetResidencyOwner(nullptr);
        }
    }

    passes.clear();
    vramHeap.ResetAll();
    halfLoaded[0] = -1;
    halfLoaded[1] = -1;
    currentPass = 0;
}

bool TextureCache::Plan(const std::vector<Texture *> &textures)
{
    ClearPlan();

    const u32 halfCapacity = vramHeap.GetHalfCapacity();
    std::vector<Pass> built;
    Pass current;

    for (size_t i = 0; i < textures.size(); ++i)
    {
        Texture *tex = textures[i];
        if (tex == nullptr)
            continue;

        if (!tex->HasPixelData())
        {
            LOG_ERROR("TextureCache::Plan on a texture with no pixel data");
            return false;
        }

        // A texture belongs to exactly one pass
        bool duplicate = std::find(current.textures.begin(), current.textures.end(), tex) != current.textures.end();
        for (size_t p = 0; p < built.size() && !duplicate; ++p)
        {
            duplicate = std::find(built[p].textures.begin(), built[p].textures.end(), tex) != built[p].textures.end();
        }
        if (duplicate)
            continue;

        const u32 words = tex->GetVramSizeWords();
        if (words == 0 || words > halfCapacity)
        {
            LOG_ERROR("TextureCache::Plan: texture needs ") << words << " words, a half holds " << halfCapacity;
            return false;
        }

        // Footprints and half size are block aligned, so summing them is exact
        if (current.words + words > halfCapacity)
        {
            built.push_back(current);
            current = Pass();
        }

        current.textures.push_back(tex);
        current.words += words;
    }

    if (!current.textures.empty())
        built.push_back(current);

    passes.swap(built);

    for (size_t p = 0; p < passes.size(); ++p)
    {
        for (size_t t = 0; t < passes[p].textures.size(); ++t)
        {
            passes[p].textures[t]->SetResidencyOwner(this);
        }
    }

    LOG_INFO("TextureCache: planned ") << passes.size() << " pass(es)";
    return true;
}

bool TextureCache::LoadPass(u32 passIndex)
{
    if (passIndex >= passes.size())
        return false;

    const u32 half = passIndex & 1u;

    // The half may still be sampled by draws a pass ago. Nothing is written
    // into it until the GS has said it is finished.
    if (fencePending && fenceHalf == half)
        WaitFence();

    Pass &pass = passes[passIndex];

    if (halfLoaded[half] != static_cast<int>(passIndex))
    {
        // The half holds a different pass (or nothing): drop it wholesale
        if (halfLoaded[half] >= 0)
        {
            Pass &old = passes[static_cast<u32>(halfLoaded[half])];
            for (size_t t = 0; t < old.textures.size(); ++t)
            {
                if (old.textures[t]->IsResident())
                    ++stats.evictions;
                old.textures[t]->OnEvicted();
            }
        }

        vramHeap.Reset(half);

        for (size_t t = 0; t < pass.textures.size(); ++t)
        {
            Texture *tex = pass.textures[t];
            const u32 address = vramHeap.Alloc(half, tex->GetVramSizeWords());
            if (address == VramHeap::INVALID)
            {
                // Plan() guarantees the pass fits, so this is a bookkeeping bug
                LOG_ERROR("TextureCache: pass ") << passIndex << " does not fit in its half";
                halfLoaded[half] = -1;
                return false;
            }
            tex->SetVramAddress(address);
        }

        halfLoaded[half] = static_cast<int>(passIndex);
    }

    // Upload whatever is not in VRAM yet. After a relayout that is every
    // texture; on a repeat frame it is only what Invalidate() flagged.
    for (size_t t = 0; t < pass.textures.size(); ++t)
    {
        Texture *tex = pass.textures[t];
        if (!tex->IsResident())
        {
            tex->TransferTextureToGS();
            ++stats.uploads;
            stats.uploadedWords += tex->GetVramSizeWords();
        }
    }

    return true;
}

void TextureCache::BeginFrame()
{
    stats = Stats();
    currentPass = 0;

    if (!passes.empty())
        LoadPass(0);
}

void TextureCache::PrefetchNextPass()
{
    const u32 next = currentPass + 1;
    if (next < passes.size())
        LoadPass(next);
}

u32 TextureCache::Bind(Texture &tex)
{
    ++stats.binds;

    if (currentPass >= passes.size())
    {
        LOG_ERROR("TextureCache::Bind outside a pass");
        return VramHeap::INVALID;
    }

    const u32 half = currentPass & 1u;
    const u32 base = vramHeap.GetHalfBase(half);

    // Resident, laid out for the pass being drawn, and inside this pass's half
    const bool inCurrentPass = halfLoaded[half] == static_cast<int>(currentPass) && tex.IsResident() &&
                               tex.GetVramAddress() >= base && tex.GetVramAddress() < base + vramHeap.GetHalfCapacity();

    if (!inCurrentPass)
    {
        LOG_ERROR("TextureCache::Bind: texture is not part of pass ") << currentPass;
        return VramHeap::INVALID;
    }

    return tex.GetVramAddress();
}

void TextureCache::EndPass()
{
    if (currentPass >= passes.size())
        return;

    // Only one fence is outstanding at a time, and CSR.FINISH has to be cleared
    // for the new one, so resolve the old one first. Normally the prefetch has
    // already done this and it returns at once.
    WaitFence();

    // EmitFence();
    fencePending = true;
    fenceHalf = currentPass & 1u;

    ++stats.fences;
    ++stats.passes;
    ++currentPass;
}

void TextureCache::Invalidate(Texture &tex)
{
    // Keep the reservation, drop only the "pixels are up to date" claim, so the
    // next LoadPass of its pass re-uploads to the same address
    if (tex.IsResident())
        tex.SetVramAddress(tex.GetVramAddress());
}

void TextureCache::Release(Texture &tex)
{
    for (size_t p = 0; p < passes.size(); ++p)
    {
        std::vector<Texture *> &list = passes[p].textures;
        std::vector<Texture *>::iterator it = std::find(list.begin(), list.end(), &tex);
        if (it == list.end())
            continue;

        passes[p].words -= tex.GetVramSizeWords();
        list.erase(it);
        break;
    }

    tex.OnEvicted();
    tex.SetResidencyOwner(nullptr);
}

u32 TextureCache::GetResidentCount() const
{
    u32 count = 0;
    for (size_t p = 0; p < passes.size(); ++p)
    {
        for (size_t t = 0; t < passes[p].textures.size(); ++t)
        {
            if (passes[p].textures[t]->IsResident())
                ++count;
        }
    }
    return count;
}

void TextureCache::DebugDump() const
{
    vramHeap.DebugDump();

    LOG_INFO("TextureCache passes=") << passes.size() << " current=" << currentPass << " resident="
                                     << GetResidentCount() << " binds=" << stats.binds << " uploads=" << stats.uploads
                                     << " evictions=" << stats.evictions << " fences=" << stats.fences
                                     << " stalls=" << stats.fenceStalls << " spins=" << stats.fenceSpins;

    for (size_t p = 0; p < passes.size(); ++p)
    {
        LOG_INFO("  pass ") << p << " half " << (p & 1) << " " << passes[p].textures.size() << " textures "
                            << passes[p].words << " words"
                            << (halfLoaded[p & 1] == static_cast<int>(p) ? " LOADED" : "");

        for (size_t t = 0; t < passes[p].textures.size(); ++t)
        {
            const Texture *tex = passes[p].textures[t];
            LOG_INFO("    tex @") << tex->GetVramAddress() << " " << tex->GetVramSizeWords() << " words"
                                  << (tex->IsResident() ? " resident" : "");
        }
    }
}

} // namespace graphics
