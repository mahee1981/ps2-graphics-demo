#ifndef SINGLE_PASS_HPP
#define SINGLE_PASS_HPP

#include "graphics/Texture.hpp"
#include "renderer/model.hpp"
#include "renderer/VramTexture.hpp"
#include <memory>
#include <vector>

namespace Renderer
{

class SinglePass
{
  private:
    std::vector<VramTexture> vramTextures;
    // TODO: create index of vramTextures to models
    u32 sizeWords = 0U;
    // No models will be added during rendering and none will be destroyed
    // during lifetime of a single pass, so reference_wrappers serves
    // the purpose
    std::vector<std::reference_wrapper<const Model>> models;

  public:
    std::shared_ptr<packet2_t> DMABuffer;

    inline u32 GetTotalVramFootprint() const
    {
        return sizeWords;
    }

    void AddTextureToPass(std::shared_ptr<Texture> texture);
    void SetModelsForPass(const std::vector<Model> &models);

    inline const std::vector<VramTexture> &GetVramTextures() const
    {
        return vramTextures;
    }
    inline std::vector<VramTexture> &GetVramTextures()
    {
        return vramTextures;
    }

    inline std::vector<std::reference_wrapper<const Model>> GetModelsInPass() const
    {
        return models;
    }

    SinglePass() = default;
};
} // namespace Renderer
#endif
