#ifndef TEXTURE_CACHE_HPP
#define TEXTURE_CACHE_HPP

#include "graphics/Texture.hpp"
#include "graphics/VramHeap.hpp"

#include <vector>

namespace graphics
{
/// Streams textures into GS local memory one pass at a time over two halves
/// of the texture area (texture_streaming_design.md, strategy A over D).
///
/// A pass is a maximal group of textures that fit in one half together. The
/// frame is an ordered sequence of passes; pass i lives in half (i & 1). While
/// VU1 draws pass i from one half on PATH1, PATH3 fills the other half with
/// pass i+1. The halves are disjoint, so nothing needs synchronising during a
/// pass. The only wait is at the swap, where the half about to be refilled must
/// be proven finished by the GS: a FINISH fence, not a DMA channel wait.
///
/// Frame loop:
///
///     cache.Plan(sortedTextures);            // once, at load
///     ...
///     cache.BeginFrame();                    // makes pass 0 resident
///     for each pass:
///         cache.Bind(tex) for each draw      // validates, returns address
///         kick the pass's packets on PATH1
///         cache.PrefetchNextPass();          // PATH3 upload into the idle half
///         cache.EndPass();                   // queues FINISH, swaps halves
///
/// Kick first, then prefetch: Texture::TransferTextureToGS blocks the EE on the
/// GIF channel, so the overlap comes from VU1 already running.
///
/// Residency is tracked across frames. Pass i always lands in the same half,
/// so a scene of one or two passes keeps everything resident and uploads
/// nothing after the first frame. With more passes, a half is simply relaid
/// out when the pass it holds is not the pass being loaded.
///
/// Draw order is NOT preserved: the caller sorts draws by texture and
/// partitions them using GetPassOf(). That is why this is for opaque geometry.
class TextureCache
{
  public:
    // Statistics for one frame, mirroring the measurements in section 6.
    struct Stats
    {
        u32 passes;        ///< EndPass() calls this frame
        u32 uploads;       ///< host to local transfers issued this frame
        u32 uploadedWords; ///< words carried by those transfers' footprints
        u32 evictions;     ///< resident textures dropped when a half was relaid out
        u32 binds;         ///< Bind() calls this frame
        u32 fences;        ///< FINISH fences queued this frame
        u32 fenceStalls;   ///< fence waits that had to spin (not already signalled)
        u32 fenceSpins;    ///< total spin iterations across those waits
    };

    /// Emits a GS FINISH register write behind all geometry already submitted.
    /// Install one that sends it through PATH1 (a VU1 program whose only job is
    /// a FINISH giftag, as tyra's RendererCoreSync does) so it queues behind
    /// everything VU1 has kicked.
    using FenceEmitter = void (*)();

    /// Manages [baseWords, baseWords + sizeWords) of GS local memory, split
    /// into two equal halves.
    ///
    /// @param baseWords First word the cache may use. Must be at or after the
    ///                  end of everything allocated through
    ///                  graph_vram_allocate, because the cache cannot see
    ///                  those allocations. Use ProbeVramBase() after the
    ///                  drawing environment has been initialized.
    /// @param sizeWords Words the cache may hand out, normally
    ///                  GRAPH_VRAM_MAX_WORDS - baseWords.
    TextureCache(u32 baseWords, u32 sizeWords);

    /// Convenience constructor: takes everything from ProbeVramBase()
    /// to the end of VRAM.
    ///
    /// Must be constructed AFTER DrawingEnvironment::InitializeEnvironment(),
    /// since that is where AllocateBuffers() moves the VRAM pointer. Build it
    /// earlier and the cache will overlap the framebuffers.
    explicit TextureCache();

    /// Detaches every planned texture so none keeps a dangling owner
    ~TextureCache();

    TextureCache(const TextureCache &other) = delete;
    TextureCache &operator=(const TextureCache &other) = delete;

    /// Reads internal VRAM bump pointer without consuming anything: a
    /// zero sized request makes graph_vram_size() return 0, so
    /// graph_vram_allocate() hands back the current pointer unchanged.
    /// That pointer is the first word not already taken by the fbuf and Zbuff
    static u32 ProbeVramBase();

    /// Partitions the textures into passes, once, greedily and in the order
    /// given: a texture that does not fit in the current pass starts a new one.
    /// Pass the textures in the order the sorted draws first reference them.
    /// Duplicates are ignored, so a texture belongs to exactly one pass.
    ///
    /// Every texture needs pixel data loaded, and must fit in one half on its
    /// own. Returns false (leaving no plan) otherwise. Replaces any earlier plan.
    bool Plan(const std::vector<Texture *> &textures);

    /// Installs the FINISH emitter. The default sends it through PATH3, which
    /// only orders against other PATH3 traffic and is NOT sufficient when
    /// geometry goes through VU1.
    void SetFenceEmitter(FenceEmitter emitter);

    /// Resets the per-frame stats and makes pass 0 resident in half 0. Does
    /// nothing if its textures are already there.
    void BeginFrame();

    /// Uploads the next pass into the idle half. Waits first on the fence for
    /// that half if one is outstanding; by then the draws are a pass old and
    /// the wait normally returns immediately. No-op on the last pass.
    void PrefetchNextPass();

    /// Returns the VRAM word address of a texture in the pass currently being
    /// drawn, or VramHeap::INVALID if it is not part of it. Call before
    /// reading GetTexGSSettings() so TEX0 is built from the real address.
    u32 Bind(Texture &tex);

    /// Closes the current pass. Call after its last packet has been kicked:
    /// queues a FINISH behind it and advances to the next pass. Call it for
    /// every pass including the last, so the next BeginFrame knows whether
    /// the half it reuses is still being read.
    void EndPass();

    /// Marks the texture for re-upload without giving up its VRAM. Use after
    /// changing the pixels in EE memory. The upload happens the next time its
    /// pass is loaded, so do not call it while that pass is being drawn.
    void Invalidate(Texture &tex);

    /// Removes the texture from the plan and forgets its address. Also called
    /// from Texture's destructor. The pass it leaves keeps its layout; the
    /// freed space is not reused until that half is relaid out.
    void Release(Texture &tex);

    /// Pass index the texture belongs to, or -1. Use at load time to
    /// partition sorted draws into passes.
    int GetPassOf(const Texture &tex) const;

    inline u32 GetPassCount() const
    {
        return static_cast<u32>(passes.size());
    }

    inline u32 GetCurrentPass() const
    {
        return currentPass;
    }

    inline const Stats &GetStats() const
    {
        return stats;
    }

    inline const VramHeap &GetVramHeap() const
    {
        return vramHeap;
    }

    /// Planned textures currently in VRAM
    u32 GetResidentCount() const;

    void DebugDump() const;

  private:
    struct Pass
    {
        std::vector<Texture *> textures;
        u32 words; ///< footprint of all textures, block aligned

        Pass() : words(0)
        {
        }
    };

    /// Makes a pass resident in its half: relays the half out if it holds
    /// something else, then uploads whatever is not yet resident.
    bool LoadPass(u32 passIndex);

    /// Drops the plan, evicts everything and resets both halves
    void ClearPlan();

    /// Clears CSR.FINISH, then queues a FINISH behind submitted geometry
    void EmitFence();

    /// Spins until CSR.FINISH is set, if a fence is outstanding
    void WaitFence();

    VramHeap vramHeap;
    std::vector<Pass> passes;
    int halfLoaded[VramHeap::HALF_COUNT]; ///< pass laid out in each half, or -1
    u32 currentPass;
    bool fencePending;
    u32 fenceHalf; ///< half whose draws the outstanding fence covers
    FenceEmitter fenceEmitter;
    Stats stats;
};
} // namespace graphics

#endif

