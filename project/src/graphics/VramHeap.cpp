#include "graphics/VramHeap.hpp"
#include "logging/log.hpp"

namespace graphics
{
VramHeap::VramHeap(u32 baseWords, u32 sizeWords) : base(0), halfCapacity(0)
{
    cursor[0] = 0;
    cursor[1] = 0;

    const u32 alignedBase = AlignUp(baseWords);
    const u32 skipped = alignedBase - baseWords;

    if (sizeWords <= skipped)
    {
        LOG_ERROR("VramHeap: region [") << baseWords << ", " << (baseWords + sizeWords)
                                        << ") is too small to hold a single GS block";
        return;
    }

    // Aligning the base consumes part of the region, so hand those words back
    // before halving. Both ends of each half stay block aligned, which is
    // what lets Alloc skip alignment handling on the cursor: an aligned start
    // plus an aligned size yields an aligned next start.
    base = alignedBase;
    halfCapacity = AlignDown((sizeWords - skipped) / HALF_COUNT);

    if (halfCapacity == 0)
    {
        LOG_ERROR("VramHeap: no usable blocks after aligning base ") << baseWords;
        base = 0;
    }
}

u32 VramHeap::Alloc(u32 half, u32 sizeWords)
{
    half &= 1u;

    // Reject against the raw size first: AlignUp would wrap around for sizes
    // near UINT_MAX and turn an absurd request into a zero-word allocation.
    if (sizeWords == 0 || sizeWords > halfCapacity)
    {
        return INVALID;
    }

    const u32 want = AlignUp(sizeWords);

    if (want > halfCapacity - cursor[half])
    {
        return INVALID;
    }

    const u32 address = GetHalfBase(half) + cursor[half];
    cursor[half] += want;
    return address;
}

void VramHeap::Reset(u32 half)
{
    cursor[half & 1u] = 0;
}

void VramHeap::ResetAll()
{
    cursor[0] = 0;
    cursor[1] = 0;
}

void VramHeap::DebugDump() const
{
    LOG_INFO("VramHeap base=") << base << " half=" << halfCapacity << " words (" << (halfCapacity * 4 / 1024) << " KB)";

    for (u32 half = 0; half < HALF_COUNT; ++half)
    {
        LOG_INFO("  half ") << half << " [" << GetHalfBase(half) << ", " << (GetHalfBase(half) + halfCapacity)
                            << ") used=" << cursor[half] << " free=" << GetHalfFree(half);
    }
}
} // namespace graphics
