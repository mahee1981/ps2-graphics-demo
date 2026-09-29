#include "graphics/VramManager.hpp"
#include "logging/log.hpp"

namespace graphics
{
VramManager::VramManager(u32 baseWords, u32 sizeWords) : base(0), capacity(0)
{
    const u32 alignedBase = AlignUp(baseWords);
    const u32 skipped = alignedBase - baseWords;

    if (sizeWords <= skipped)
    {
        LOG_ERROR("VramManager: region [")
            << baseWords << ", " << (baseWords + sizeWords) << ") is too small to hold a single GS block";
        return;
    }

    // Aligning the base consumes the part of the region, so hand those words back
    // before rounding the usable size down to whole blocks. Keeping both ends
    // aligned is what lets Alloc/Free skip alignment handling entirely: an
    // aligned start plus an aligned size yields an aligned next start, so the
    // invariant holds across every split and merge.
    base = alignedBase;
    capacity = AlignDown(sizeWords - skipped);

    if (capacity == 0)
    {
        LOG_ERROR("VramManager: no usable blocks after aligning base ") << baseWords;
        return;
    }

    blocks.push_back(Block{base, capacity, false});
}

u32 VramManager::Alloc(u32 sizeWords)
{
    // Reject agains the raw size first: AlignUp would wrap around for sizes
    // near UINT_MAX and turn an absurd request into a zero-word allocation.
    if (sizeWords == 0 || sizeWords > capacity)
    {
        return INVALID;
    }

    const u32 want = AlignUp(sizeWords);

    if (want > capacity)
        return INVALID;

    for (BlockList::iterator it = blocks.begin(); it != blocks.end(); ++it)
    {
        if (it->used || it->size < want)
            continue;

        // Split only when there is a remainder, so exact fits do not leave zero-sized blocks in the list
        if (it->size > want)
        {
            BlockList::iterator next = it;
            ++next;
            blocks.insert(next, Block{it->start + want, it->size - want, false});
        }

        it->size = want;
        it->used = true;
        return it->start;
    }

    return INVALID;
}

void VramManager::Free(u32 address)
{
    for (BlockList::iterator it = blocks.begin(); it != blocks.end(); ++it)
    {
        if (it->start != address)
            continue;

        if (!it->used)
        {
            LOG_ERROR("VramManager:: double free at word ") << address;
        }

        it->used = false;
        ColapseFreeBlocks(it);
        return;
    }

    LOG_ERROR("Tried to free unknown address");
}

void VramManager::Reset()
{
    blocks.clear();

    if (capacity > 0)
        blocks.push_back(Block{base, capacity, false});
}

void VramManager::ColapseFreeBlocks(BlockList::iterator it)
{
    // Walk back to the first free block of the run...
    while (it != blocks.begin())
    {
        BlockList::iterator prev = it;
        --prev;
        if (prev->used)
            break;
        it = prev;
    }

    // ...then absorb every free block that follows
    BlockList::iterator next = it;
    ++next;
    while (next != blocks.end() && !next->used)
    {
        it->size += next->size;
        next = blocks.erase(next);
    }
}

u32 VramManager::LargestFreeRun() const
{
    u32 largest = 0;

    for (BlockList::const_iterator it = blocks.begin(); it != blocks.end(); ++it)
    {
        if (!it->used && it->size > largest)
            largest = it->size;
    }

    return largest;
}

u32 VramManager::TotalFree() const
{
    u32 total = 0;

    for (BlockList::const_iterator it = blocks.begin(); it != blocks.end(); ++it)
    {
        if (!it->used)
            total += it->size;
    }

    return total;
}

void VramManager::DebugDump() const
{
    LOG_INFO("VramManager base=") << base << " capacity=" << capacity << " free=" << TotalFree() << " largest"
                                  << LargestFreeRun() << " blocks=" << GetBlockCount();

    for (BlockList::const_iterator it = blocks.begin(); it != blocks.end(); ++it)
    {
        LOG_INFO("  [") << it->start << ", " << (it->start + it->size) << ") " << it->size << " words "
                        << (it->used ? "used" : "free");
    }
}
} // namespace graphics
