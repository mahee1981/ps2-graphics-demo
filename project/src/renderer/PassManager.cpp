#include "renderer/PassManager.hpp"
#include "dma.h"
#include "gs_psm.h"
#include "logging/log.hpp"
#include "packet2.h"
#include "packet2_chain.h"
#include "packet2_types.h"
#include <memory>

namespace Renderer
{
namespace
{
/// GS privileged register CSR, and its FINISH event bit. Writing 1 to the bit
/// clears it; it reads back as 1 once a FINISH register write has been processed.
volatile u64 *const GS_CSR = reinterpret_cast<volatile u64 *>(0x12001000);
constexpr u64 CSR_FINISH = u64(1) << 1;

/// Gives up spinning on a fence that never arrives rather than hanging the console
constexpr u32 MAX_FENCE_SPINS = 50000000u;

} // namespace

bool PassManager::GenerateRenderingPasses(const std::vector<Model> &models)
{
    ClearPlan();
    auto newPasses = std::vector<SinglePass>();
    const u32 halfCapacity = vramHeap.GetHalfCapacity();
    SinglePass currentPass;

    for (const Model &model : models)
    {
        const std::shared_ptr<Texture> texturePtr = model.GetTextureById(0);
        
        // TODO: handle models without textures and take this logic out
        if (texturePtr == nullptr)
            return false;

        if (!texturePtr->HasPixelData())
        {
            LOG_ERROR("Plan on a texture with no pixel data");
            return false;
        }
        const u32 words = texturePtr->GetVramSizeWords();
        if (words == 0 || words > halfCapacity)
        {
            LOG_ERROR("Plan: texture needs ") << words << " words, a half holds " << halfCapacity;
            return false;
        }

        if (currentPass.GetTotalVramFootprint() + words > halfCapacity)
        {
            newPasses.push_back(std::move(currentPass));
            currentPass = SinglePass();
        }
        currentPass.AddTextureAndModel(texturePtr, model);
    }

    if (currentPass.GetTotalVramFootprint() != 0U)
    {
        newPasses.push_back(std::move(currentPass));
    }

    newPasses.swap(passCollection);
    return true;
}

bool PassManager::UploadTexturesFromPass(std::size_t passIndex)
{
    if (passIndex >= passCollection.size())
        return false;

    const u32 half = passIndex & 1u;

    SinglePass &pass = passCollection[passIndex];

    if (halfLoaded[half] != static_cast<int>(passIndex))
    {
        // The half holds a different pass (or nothing): drop it wholesale
        if (halfLoaded[half] >= 0)
        {
            SinglePass &oldPass = passCollection[static_cast<u32>(halfLoaded[half])];
            for (VramTexture &vramTexture : oldPass.GetVramTextures())
            {
                if (vramTexture.IsResident())
                    ++stats.evictions;
                vramTexture.OnEvicted();
            }
        }

        vramHeap.Reset(half);

        for (VramTexture &vramTexture : pass.GetVramTextures())
        {
            const u32 address = vramHeap.Alloc(half, vramTexture.GetVramSizeWords());
            if (address == VramHeap::INVALID)
            {
                // Plan() guarantees the pass fits, so this is a bookkeeping bug
                LOG_ERROR("pass ") << passIndex << " does not fit in its half";
                halfLoaded[half] = -1;
                return false;
            }
            vramTexture.SetVramAddress(address);
        }

        halfLoaded[half] = static_cast<int>(passIndex);
    }

    // Upload whatever is not in VRAM yet.
    packet2_t* currentDmaBuffer = dynamicPacket[half].get();
    packet2_reset(currentDmaBuffer, 0);

    for (VramTexture &vramTexture : pass.GetVramTextures())
    {
        if (!vramTexture.IsResident())
        {
            auto textureDisplayList = vramTexture.GetDMAPacket();
            packet2_chain_call(currentDmaBuffer, (const void *)(textureDisplayList.get()->base), 0, 0, 0);
            packet2_chain_close_tag(currentDmaBuffer);
            ++stats.uploads;
            stats.uploadedWords += vramTexture.GetVramSizeWords();
            vramTexture.SetResident();
        }
    }
    // Generate FINISH event on texture upload
    packet2_chain_open_end(currentDmaBuffer, 0, 0);
    packet2_update(currentDmaBuffer, draw_finish(currentDmaBuffer->next));
    packet2_chain_close_tag(currentDmaBuffer);

    fencePending = true;
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    dma_channel_send_packet2(currentDmaBuffer, DMA_CHANNEL_GIF, 1);


    return true;
}

void PassManager::BeginFrame()
{
    stats = Stats();
    currentPassIdx = 0;

    if (!passCollection.empty())
        UploadTexturesFromPass(0);
}

void PassManager::PrefetchNextPassTextures()
{
    const u32 next = currentPassIdx + 1;
    if (next < passCollection.size())
        UploadTexturesFromPass(next);
}

void PassManager::EndPass()
{
    if (currentPassIdx >= passCollection.size())
        return;

    ++stats.passes;
    ++currentPassIdx;
}

void PassManager::WaitFence()
{
    if (!fencePending)
        return;

    u32 spins = 0;
    while ((*GS_CSR & CSR_FINISH) == 0)
    {
        if (++spins >= MAX_FENCE_SPINS)
        {
            LOG_ERROR("gave up waiting for GS FINISH, the fence emitter may not reach the GS");
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

PassManager::PassManager(u32 baseWords, u32 sizeWords)
    : vramHeap(baseWords, sizeWords), stats(), currentPassIdx(0), fencePending(false), 
      dynamicPacket{SmartPacket{packet2_create(30, P2_TYPE_NORMAL, P2_MODE_CHAIN, 0), &packet2_free},
                    SmartPacket{packet2_create(30, P2_TYPE_NORMAL, P2_MODE_CHAIN, 0), &packet2_free}}
{
    halfLoaded[0] = -1;
    halfLoaded[1] = -1;

    LOG_INFO("Cache base=") << vramHeap.GetBase() << " words, 2 halves of " << vramHeap.GetHalfCapacity() << " words ("
                            << (vramHeap.GetHalfCapacity() * 4 / 1024) << " KB) each";
}

PassManager::PassManager() : PassManager(ProbeVramBase(), GRAPH_VRAM_MAX_WORDS - ProbeVramBase())
{
}

PassManager::~PassManager()
{
    for (SinglePass &pass : passCollection)
    {
        for (VramTexture &vramTexture : pass.GetVramTextures())
        {
            vramTexture.OnEvicted();
        }
    }
}

u32 PassManager::ProbeVramBase()
{
    const int base = graph_vram_allocate(0, 0, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    LOG_INFO("GRAPH VRAM ALLOC returned ") << base;
    if (base < 0)
        LOG_INFO("Something went wrong during probing");
    return base > 0 ? static_cast<u32>(base) : 0u;
}

void PassManager::ClearPlan()
{
    passCollection.clear();
    vramHeap.ResetAll();
    halfLoaded[0] = -1;
    halfLoaded[1] = -1;
    currentPassIdx = 0;
}

u32 PassManager::GetResidentCount() const
{
    u32 count = 0;
    for (const SinglePass &pass : passCollection)
    {
        for (const VramTexture &vramTexture : pass.GetVramTextures())
        {
            if (vramTexture.IsResident())
                ++count;
        }
    }
    return count;
}

void PassManager::DebugDump() const
{
    vramHeap.DebugDump();

    LOG_INFO("passes=") << passCollection.size() << " current=" << currentPassIdx << " resident=" << GetResidentCount()
                        << " uploads=" << stats.uploads << " evictions=" << stats.evictions
                        << " fences=" << stats.fences << " stalls=" << stats.fenceStalls
                        << " spins=" << stats.fenceSpins;

    // for (size_t p = 0; p < passes.size(); ++p)
    // {
    //     LOG_INFO("  pass ") << p << " half " << (p & 1) << " " << passes[p].textures.size() << " textures "
    //                         << passes[p].words << " words"
    //                         << (halfLoaded[p & 1] == static_cast<int>(p) ? " LOADED" : "");
    //
    //     for (size_t t = 0; t < passes[p].textures.size(); ++t)
    //     {
    //         const Texture *tex = passes[p].textures[t];
    //         LOG_INFO("    tex @") << tex->GetVramAddress() << " " << tex->GetVramSizeWords() << " words"
    //                               << (tex->IsResident() ? " resident" : "");
    //     }
    // }
}

} // namespace Renderer
