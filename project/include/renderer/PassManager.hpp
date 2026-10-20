#ifndef SINGLE_PASS_COLLECTION
#define SINGLE_PASS_COLLECTION

#include "graphics/VramHeap.hpp"
#include <functional>
#include <memory>
#include <renderer/model.hpp>

namespace Renderer {

class SinglePass
{
  private:
    std::vector<std::shared_ptr<Texture>> textures;
    u32 sizeWords = 0U;


  public:
    std::vector<std::reference_wrapper<const Model>> models;
    std::shared_ptr<packet2_t> DMABuffer;

    inline u32 GetTotalVramFootprint() const { return sizeWords; }

    void AddModelToPass(const Model &model, u32 textureSize);

    SinglePass() = default;
};

class PassManager
{
  public:
    bool GenerateRenderingPasses(const std::vector<Model> &models);
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

    inline u32 GetPassCount() const
    {
        return static_cast<u32>(passCollection.size());
    }

    inline SinglePass GetCurrentPass() const
    {
        return passCollection[currentPassIdx];
    }

    inline const Stats &GetStats() const
    {
        return stats;
    }

    void BeginFrame();
    void PrefetchNextPassTextures();
    void EndPass();
    void ClearPlan();
    void WaitFence();
    void DebugDump() const;

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
    PassManager(u32 baseWords, u32 sizeWords);

    /// Convenience constructor: takes everything from ProbeVramBase()
    /// to the end of VRAM.
    ///
    /// Must be constructed AFTER DrawingEnvironment::InitializeEnvironment(),
    /// since that is where AllocateBuffers() moves the VRAM pointer. Build it
    /// earlier and the cache will overlap the framebuffers.
    explicit PassManager();

    /// Detaches every planned texture so none keeps a dangling owner
    ~PassManager();

    PassManager(const PassManager &other) = delete;
    PassManager &operator=(const PassManager &other) = delete;


  private:
    VramHeap vramHeap;
    std::vector<SinglePass> passCollection;
    bool UploadTexturesFromPass(std::size_t index);

    /// Reads internal VRAM bump pointer without consuming anything: a
    /// zero sized request makes graph_vram_size() return 0, so
    /// graph_vram_allocate() hands back the current pointer unchanged.
    /// That pointer is the first word not already taken by the fbuf and Zbuff
    static u32 ProbeVramBase();
    u32 GetResidentCount() const;

    int halfLoaded[VramHeap::HALF_COUNT]; ///< pass laid out in each half, or -1
    Stats stats;
    u32 currentPassIdx;
    bool fencePending;
    // Take all the textures currently loaded
    // make passes based on the textures that can fit into one half of remaining VRAM
    // use the picked textures to pick the models and meshes
    // render as usual and sync on done
};
}

#endif
