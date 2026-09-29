#include "graphics/TextureCache.hpp"
#include "graph_vram.h"
#include "graphics/VramManager.hpp"
#include "logging/log.hpp"

#include <dma.h>
#include <gs_psm.h>
#include <iterator>
#include <locale>
#include <vector>


namespace graphics
{
TextureCache::TextureCache(u32 baseWords, u32 sizeWords) : vramManager(baseWords, sizeWords), stats{0,0,0}
{
    entries.reserve(32);

    LOG_INFO("Texture cache base=") << vramManager.GetBase() << " words, capacity=" << vramManager.GetCapacity() << " words ("
        << (vramManager.GetCapacity() * 4 / 1024) << " KB)";
}

TextureCache::TextureCache() : TextureCache(ProbeVramBase(), GRAPH_VRAM_MAX_WORDS - ProbeVramBase())
{
}

u32 TextureCache::ProbeVramBase() 
{
    const int base = graph_vram_allocate(0, 0, GS_PSM_32, GRAPH_ALIGN_BLOCK);
    return base > 0 ? static_cast<u32>(base) : 0u;
}

TextureCache::Entry *TextureCache::Find(const Texture &tex)
{
    for(auto it = entries.begin(); it!=entries.end(); ++it)
    {
        if (it->tex == &tex)
            return &(*it);
    }
    return nullptr;
}

u32 TextureCache::Weight(const Entry &entry)
{
    if (entry.useCount == entry.useCountPrev)
    {
        // Done for this frame, but last frame's count says it comes back
        return entry.useCountPrev;
    }

    if (entry.useCount < entry.useCountPrev)
    {
        // Fewer binds so far than last frame: still needed this frame, and
        // again next frame. The most expensive thing to lose
        return (entry.useCountPrev - entry.useCount) + entry.useCountPrev;
    }

    // More binds than last frame: unsure about the rest of this frame, but it
    // is clearly in use
    return 1 + entry.useCount;
}

void TextureCache::RemoveEntry(Entry *entry)
{
    *entry = entries.back();
    entries.pop_back();
}

bool TextureCache::EvictOne()
{
    Entry *victim = nullptr;
    for (std::vector<Entry>::iterator it = entries.begin(); it != entries.end(); ++it)
    {

        if (it->useCount > 0)
        {
            continue;
        }

        if (victim == nullptr || Weight(*it) < Weight(*victim))
        {
            victim = &(*it);
        }
    }

    if (victim == nullptr)
        return false;

    vramManager.Free(victim->address);
    victim->tex->OnEvicted();
    RemoveEntry(victim);
    ++stats.evictions;
    return true;
}

u32 TextureCache::Bind(Texture &tex)
{
    ++stats.binds;

    Entry *entry = Find(tex);

    if (entry == nullptr)
    {
        if (!tex.HasPixelData())
        {
            LOG_ERROR("TextureCache::Bind on a texture wiht no pixel data");
            return VramManager::INVALID;
        }

        const u32 sizeWords = tex.GetVramSizeWords();
        u32 address = vramManager.Alloc(sizeWords);

        // Give up the cheapest texture, retry, repeat. Freeing a neighbour
        // merges holes in the heap, so this also handles the case where one
        // large texture needs the space of several small ones.
        while (address == VramManager::INVALID)
        {
            if (!EvictOne())
            {
                LOG_ERROR("TextureCache: cannot fit ") << sizeWords << " words, largest free run is "
                    << vramManager.LargestFreeRun() << " words ("
                    << GetResidentCount() << " resident, all pinned)";
                return VramManager::INVALID;
            }

            address = vramManager.Alloc(sizeWords);

        }
        tex.SetVramAddress(address, this);

        entries.push_back(Entry{&tex, address, sizeWords, 0, 1});
        entry = &entries.back();
    }

    if (!tex.IsResident())
    {
        // The upload goes out on PATH3 while VU1 draws go out on PATH1. Wait for
        // the draw channel first, otherwise we may overwrite VRAM that queued
        // primitives are still reading.
        dma_channel_wait(DMA_CHANNEL_VIF1, 0);
        tex.TransferTextureToGS();
        ++stats.uploads;
    }
    ++entry->useCount;
    return entry->address;
}

void TextureCache::Invalidate(Texture &tex)
{
    Entry *entry = Find(tex);
    if (entry == nullptr)
        return;

    // Keep the VRAM reservation, drop only the "pixels are up to date" claim,
    // so the next Bind() re-uploads to the same address
    tex.SetVramAddress(entry->address, this);
}

void TextureCache::Release(Texture &tex)
{
    Entry *entry = Find(tex);
    if (entry == nullptr)
        return;

    vramManager.Free(entry->address);
    tex.OnEvicted();
    RemoveEntry(entry);
}

void TextureCache::NextFrame()
{
    for (std::vector<Entry>::iterator it = entries.begin(); it != entries.end(); ++it)
    {
        it->useCountPrev = it->useCount;
        it->useCount = 0;
    }

    stats.uploads = 0;
    stats.evictions = 0;
    stats.binds = 0;
}

void TextureCache::DebugDump() const
{
    vramManager.DebugDump();

    LOG_INFO("TextureCache resident=") << GetResidentCount() << " binds=" << stats.binds 
        << " uploads=" << stats.uploads << " evictions=" << stats.evictions;

    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        LOG_INFO("  tex @") << it->address << " " << it->sizeWords << " words use=" << it->useCount << " prev="
            << it->useCountPrev << " weight=" << Weight(*it) << (it->useCount > 0 ? " PINNED" : "");
    }
}

}
