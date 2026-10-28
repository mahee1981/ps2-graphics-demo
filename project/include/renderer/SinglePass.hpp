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
    using Texture = graphics::Texture;
  private:
    std::vector<VramTexture> vramTextures;
    std::vector<std::size_t> textureToModelIndex; 
    // No models will be added during rendering and none will be destroyed
    // during lifetime of a single pass, so reference_wrappers serves
    // the purpose
    std::vector<std::reference_wrapper<const Model>> models;
    u32 sizeWords = 0U;
    // Gives index into vramTextures matching which matches the model at index i
    inline const VramTexture &GetVramTextureForModelIndex(std::size_t idx) const;
    int IndexOfVramTextureByTextureId(const u32 textureId) const;

  public:

    inline u32 GetTotalVramFootprint() const
    {
        return sizeWords;
    }

    void AddTextureAndModel(const std::shared_ptr<Texture> &texture, const Model &model);

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


    u64 GetModelTextureBindSettings(const Model &model) const;

    SinglePass() = default;

    void DumpPassInfo() const;
};
} // namespace Renderer
#endif
