#ifndef VRAM_MGR_HPP
#define VRAM_MGR_HPP

#include "tamtypes.h"
#include <graph_vram.h>
#include <list>
namespace graphics
{
/// First-fit free-list allocator for a fixed region of GS local memory
///
/// All addresses and sizes are in 32-bit words, matching ps2sdk's graph_vram
/// convention (GRPAH_VRAM_MAX_WORDS == 4MB / 4). This is NOT the same unit
/// gsKitUses, where VRAM offsets are byte offsets.
///
/// Every address handed out is a multiple of GRAPH_ALIGN_BLOCK
/// (64 words / 256 bytes / one GS block), which is the granularity te TBP0 field of TEX0
/// can address. An unaligned base would be silenty truncated by the 'address >> 6' that
/// builds TEX0, so the alignment is a correctness requirement rather than
/// an optimization.
///
/// The heap knows nothing about textures, DMA or GS registers, it is integer
/// bookkeeping only. Deciding *what* deserves to be resident and evicting on
/// failure, belongs one layer up.
class VramManager
{
  public:
    /// Returned by Alloc when no single free run is large enough
    static constexpr u32 INVALID = ~0;
    /// @param baseWords First word this heap may use. Must be at or after the
    ///                  end of every buffer allocated through
    ///                  graph_vram_allocate (fbuf, zbuf), since
    ///                  the heap cannot see those. Rounded up to a block
    ///                  boundary if needed.
    /// @param sizeWords Number of words, starting at baseWords, the heap may
    ///                  hand out. Usually GRAPH_VRAM_MAX_WORDS - baseWords;
    ///                  pass less to keep a tail region under manual control.
    ///                  Rounded down to a whole number of blocks.
    VramManager(u32 baseWords, u32 sizeWords);

    VramManager(const VramManager &other) = delete;
    VramManager &operator=(const VramManager &other) = delete;

    /// Reserves sizeWords (rounded up to a block) and returns its address, or
    /// INVALID when no contiguous free run is large enough. Callers are
    /// expected to handle INVALID: this allocator never evicts on its own
    u32 Alloc(u32 sizeWords);

    /// Releases a block previously returned by Alloc and merges it with any
    /// free neighbors. Passing an address that is not the start of a live
    /// block is ignore (and logged)
    void Free(u32 address);

    /// Drops every allocation and returns to a single free run
    void Reset();

    /// Largest allocation that can currently succeed
    u32 LargestFreeRun() const;

    /// Total free words, which may exceed LargestFreeRun() when fragmented
    u32 TotalFree() const;

    inline u32 GetBase() const
    {
        return base;
    }

    inline u32 GetCapacity() const
    {
        return capacity;
    }

    /// Number of blocks in the list, free and used. A cheap fragmentation
    /// metric: it grows as the free space breaks up
    inline u32 GetBlockCount() const
    {
        return static_cast<u32>(blocks.size());
    }

    /// Prints the block list. Debug aid, not cheap.
    void DebugDump() const;

  private:
    static_assert((GRAPH_ALIGN_BLOCK & (GRAPH_ALIGN_BLOCK - 1)) == 0, "GRAPH_ALIGN_BLOCK must be a power of two");

    struct Block
    {
        u32 start;
        u32 size;
        bool used;
    };

    using BlockList = std::list<Block>;

    /// Rounds up to the next block boundary. Relies on GRPAH_ALIGN_BLOCK
    /// being a power of two: add size-1, then mask the remainder away.
    /// Necessary only if I do something werid like specify the size myself
    /// and not use graph_vram_size. Also used during initialization since
    /// current pointer might not be aligned on GRAPH_ALIGN_BLOCK
    static inline u32 AlignUp(u32 words)
    {
        return (words + GRAPH_ALIGN_BLOCK - 1) & ~(static_cast<u32>(GRAPH_ALIGN_BLOCK) - 1);
    }

    static inline u32 AlignDown(u32 words)
    {
        return words & ~(static_cast<u32>(GRAPH_ALIGN_BLOCK) - 1);
    }

    /// Merges the run of free block containing 'it' into one block.
    void ColapseFreeBlocks(BlockList::iterator it);

    BlockList blocks;
    u32 base;
    u32 capacity;
};
} // namespace graphics

#endif
