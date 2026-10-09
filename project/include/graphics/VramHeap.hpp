#ifndef VRAM_HEAP_HPP
#define VRAM_HEAP_HPP

#include "tamtypes.h"
#include <graph_vram.h>

namespace graphics
{
/// Split-window bump allocator for the texture area of GS local memory.
///
/// The texture area is divided into two equal halves so that PATH3 can upload
/// the next pass's textures into one half while VU1 draws from the other. The
/// two halves never overlap, which is what makes concurrent upload and draw
/// safe without any synchronisation during a pass (design doc 2.3).
///
/// Each half is a plain bump region: allocation is a bounds check and an add,
/// and there is no individual free, only a wholesale Reset(half). That matches
/// pass-based reclaim, where everything in a half dies together (4.1). There
/// is no fragmentation to manage and no per-texture lifetime to track.
///
/// All addresses and sizes are in 32-bit words, matching ps2sdk's graph_vram
/// convention (GRAPH_VRAM_MAX_WORDS == 4MB / 4). 
/// 
/// Every address handed out is a multiple of GRAPH_ALIGN_BLOCK (64 words /
/// 256 bytes / one GS block), the granularity of the TBP0 field of TEX0. An
/// unaligned base would be silently truncated by the 'address >> 6' that builds
/// TEX0, so alignment is a correctness requirement. Base, half size and every
/// allocation are aligned, so each cursor position stays aligned.
///
/// The heap knows nothing about textures, DMA or GS registers. Deciding
/// what goes in which half, and when a half may be reset, is the cache's job.
class VramHeap
{
  public:
    /// Returned by Alloc when the half cannot hold the request
    static constexpr u32 INVALID = ~0u;

    static constexpr u32 HALF_COUNT = 2;

    /// @param baseWords First word of the texture area. Must be at or after
    ///                  the end of every buffer allocated through
    ///                  graph_vram_allocate (fbuf, zbuf), since this heap
    ///                  cannot see those. Rounded up to a block boundary.
    /// @param sizeWords Words, starting at baseWords, the texture area may
    ///                  use. Split into two equal halves, each rounded down
    ///                  to a whole number of blocks. Pass less than
    ///                  GRAPH_VRAM_MAX_WORDS - baseWords to keep a tail
    ///                  region under manual control.
    VramHeap(u32 baseWords, u32 sizeWords);

    VramHeap(const VramHeap &other) = delete;
    VramHeap &operator=(const VramHeap &other) = delete;

    /// Reserves sizeWords (rounded up to a block) in the given half and
    /// returns its address, or INVALID when it does not fit. Never evicts.
    u32 Alloc(u32 half, u32 sizeWords);

    /// Drops every allocation in one half. The caller must already know the
    /// GS has finished reading that half (a fence).
    void Reset(u32 half);

    void ResetAll();

    /// First word of a half
    inline u32 GetHalfBase(u32 half) const
    {
        return base + (half & 1u) * halfCapacity;
    }

    /// Words in one half
    inline u32 GetHalfCapacity() const
    {
        return halfCapacity;
    }

    inline u32 GetHalfUsed(u32 half) const
    {
        return cursor[half & 1u];
    }

    inline u32 GetHalfFree(u32 half) const
    {
        return halfCapacity - cursor[half & 1u];
    }

    /// First word of the texture area, block aligned
    inline u32 GetBase() const
    {
        return base;
    }

    /// Total words usable across both halves
    inline u32 GetCapacity() const
    {
        return halfCapacity * HALF_COUNT;
    }

    /// Prints both halves. Debug aid.
    void DebugDump() const;

  private:
    static_assert((GRAPH_ALIGN_BLOCK & (GRAPH_ALIGN_BLOCK - 1)) == 0, "GRAPH_ALIGN_BLOCK must be a power of two");

    /// Rounds up to the next block boundary. Relies on GRAPH_ALIGN_BLOCK
    /// being a power of two: add size-1, then mask the remainder away.
    static inline u32 AlignUp(u32 words)
    {
        return (words + GRAPH_ALIGN_BLOCK - 1) & ~(static_cast<u32>(GRAPH_ALIGN_BLOCK) - 1);
    }

    static inline u32 AlignDown(u32 words)
    {
        return words & ~(static_cast<u32>(GRAPH_ALIGN_BLOCK) - 1);
    }

    u32 base;
    u32 halfCapacity;
    u32 cursor[HALF_COUNT];
};
} // namespace graphics

#endif
