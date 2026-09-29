#ifndef TEXTURE_CACHE_HPP
#define TEXTURE_CACHE_HPP

#include "graphics/Texture.hpp"
#include "graphics/VramManager.hpp"

#include <vector>

namespace graphics
{
/// Keeps as many textures resident in GS local memory as will fit, and decides
/// which ones to throw out when they do not.
///
/// This is the policy layer on top of VramManager, split the way gsKit's
/// gsTexManager is not: the manager answers "is this the rang of VRAM in use", the
/// cache answers "which textures deserve to be in VRAM right now". The only
/// coupling is the retry loop in Bind(), where an allocation failure is turned
/// into an eviction.
///
/// Residency state lives in the Texture, not here, exactly as gsKit keeps it in
/// GSTEXTURE::Vram. Invalidating a texture is therefore free, and Bind() on an
/// already resident texture costs a linear scan and a counter bump
///
/// Eviction uses gsKit's prediction rather than plain LRU (least recently used)
/// A texture that was used N times last frame and has not been used yet this frame
/// is assumed to still be needed, so it is protected. On top of that, anything already
/// bound during the current frame is pinned, and cannot be evicted at all, because
/// primitives referencing it may not have reached GS yet. gsKit's ETM_DIRECT mode
/// has exactly this hazard and does nto guard against it.

class TextureCache
{
  public:
    // Statistics for one frame.
    struct Stats
    {
        u32 uploads;   ///< host to local trasnfers issued this frame
        u32 evictions; ///< textures thrown out this frame
        u32 binds;     ///< Bind() calls this frame
    };

    /// Manages [baseWords, baseWords + sizeWords] of GS local memory.
    ///
    /// @param baseWords First word the cache may use in 32-bit words. Must be
    ///                  at or after the end of everything allocated through
    ///                  graph_vram_allocate, because the cache cannot see
    ///                  those allocations. Use ProbeVramBase() after the
    ///                  drawing environment has been initialized.
    /// @oaram sizeWords Words the cache may hand out, normally
    ///                  GRAPH_VRAM_MAX_WORDS - baseWords. Pass less to keep a
    ///                  tail region under manual control.
    TextureCache(u32 baseWords, u32 sizeWords);

    /// Convenience constructor: takes everything from ProbeVramBase()
    /// to the end of VRAM.
    ///
    /// Must be constructed AFTER DrawingEnvironment::InitializeEnvironment()m
    /// since that is where AllocateBuffers() moves VRAM pointer. Build
    /// it cache and the cache will overlap the framebuffers.
    explicit TextureCache();

    TextureCache(const TextureCache &other) = delete;
    TextureCache &operator=(const TextureCache &other) = delete;

    /// Reads internal VRAM bump pointer without consuming anything: a
    /// zero sized request makes graph_vram_size() return 0, so
    /// graph_vram_allocate() hands back the current pointer unchanged.
    /// That pointer is the first word not already taken by the fbuf and Zbuff
    static u32 ProbeVramBase();

    /// Makes tex resident, uploading it if necessary, and returns its VRAM
    /// word address. Returns VramManager::INVALID when it cannot be made resident,
    /// which means the texutre is larger than the whole heap or everything else
    /// is pinned this frame.
    ///
    /// Must be called before GetTexGSSettings() is read, since eviction can
    /// move a texture between frames and TEX0 has to be rebuilt from the new
    /// address
    u32 Bind(Texture &tex);

    /// Marks the texture for re-upload on the next Bind without giving up its
    /// VRAM. Use after changing the pixels in EE memory.
    void Invalidate(Texture &tex);

    /// Hands the VRAM back immediately instead of waiting for an eviction. Also
    /// called from Texture's destructor so the cache never keeps a dangling
    /// entry
    void Release(Texture &tex);

    /// Rolls the per-frame use counters over and unpins everything. Call once
    /// per frame, after the frame's draws have been flushed.
    void NextFrame();

    inline const Stats &GetStats() const
    {
        return stats;
    }

    inline const VramManager &GetVramManager() const
    {
        return vramManager;
    }

    inline u32 GetResidentCount() const
    {
        return static_cast<u32>(entries.size());
    }

    void DebugDump() const;

  private:
    struct Entry
    {
        Texture *tex;
        u32 address;
        u32 sizeWords;
        u32 useCount;     ///< binds so far this frame
        u32 useCountPrev; ///< binds during the previous frame
    };

    Entry *Find(const Texture &tex);

    /// How much this entry is worth keeping. Higher means more valuable.
    static u32 Weight(const Entry &entry);

    /// Frees the least valuable entry that is not pinned this frame. Returns
    /// false when every remaining entry is pinned, which is the signal to give up
    /// rather than spin. Bind() calls this repeatedly so a large texture can reclaim
    /// the space of several small ones as freed neightbours merge in the heap.
    bool EvictOne();

    void RemoveEntry(Entry *entry);

    VramManager vramManager;
    std::vector<Entry> entries;
    Stats stats;
};
} // namespace graphics

#endif
