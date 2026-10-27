#include "renderer/SinglePass.hpp"
#include "logging/log.hpp"
#include "renderer/VramTexture.hpp"

namespace Renderer
{

void SinglePass::AddTextureAndModel(const std::shared_ptr<Texture> &texturePtr, const Model &model)
{
    int vramTextureIdx = IndexOfVramTextureByTextureId(texturePtr->GetId());

    if (vramTextureIdx == -1)
    {
        vramTextures.emplace_back(texturePtr);
        sizeWords += texturePtr->GetVramSizeWords();
        textureToModelIndex.push_back(vramTextures.size() - 1);
    }
    else
    {
        textureToModelIndex.push_back(vramTextureIdx);
    }

    models.emplace_back(model);
}

u64 SinglePass::GetModelTextureBindSettings(const Model &model) const
{
    u32 id = model.GetId();

    for (std::size_t i = 0; i < models.size(); i++)
    {
        if (models[i].get().GetId() == id)
        {
            return GetVramTextureForModelIndex(i).GetTextureActiveSettings();
        }
    }

    // TODO: handle gracefully
    LOG_ERROR("You fucked up");
    return 0;
}
const VramTexture &SinglePass::GetVramTextureForModelIndex(std::size_t idx) const
{
    return vramTextures[textureToModelIndex[idx]];
}

int SinglePass::IndexOfVramTextureByTextureId(const u32 textureId) const
{
    for (std::size_t i = 0; i < vramTextures.size(); ++i)
    {
        if (textureId == vramTextures[i].GetTextureDataId())
            return i;
    }
    return -1;

}

void SinglePass::DumpPassInfo() const
{
    LOG_INFO("Current pass info: number of textures: ") << vramTextures.size() << ", number of models: " << models.size(); 
}

}
