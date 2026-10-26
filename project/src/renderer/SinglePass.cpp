#include <algorithm>
#include "renderer/SinglePass.hpp"
#include "logging/log.hpp"
#include "renderer/VramTexture.hpp"

namespace Renderer
{

void SinglePass::AddTextureToPass(std::shared_ptr<Texture> texturePtr)
{
    vramTextures.push_back(texturePtr);
    sizeWords += texturePtr->GetVramSizeWords();
}
// TODO: switch to a map maybe??
void SinglePass::SetModelsForPass(const std::vector<Model> &models)
{
    for (const Model& model : models)
    {
        const std::shared_ptr<Texture> &modelTexture = model.GetTextureById(0);
        if (std::find_if(vramTextures.begin(), vramTextures.end(), [&modelTexture](const VramTexture &vramTexture) -> bool {
            return vramTexture.GetTextureDataId() == modelTexture->GetId(); }) != vramTextures.end())
        {
            this->models.emplace_back(model); 
        }
    }
}

u64 SinglePass::GetModelTextureBindSettings(const Model &model) const
{
    u32 modelsTextureId = model.GetTextureById(0)->GetId();
    if (auto result = std::find_if(vramTextures.begin(),
                                   vramTextures.end(),
                                   [&modelsTextureId](const VramTexture &vramTexture) -> bool {
                                       return vramTexture.GetTextureDataId() == modelsTextureId;
                                   });  result != vramTextures.end())
    {
        return result->GetTextureActiveSettings();
    }

    // Handle this with grace
    LOG_INFO("Returing 0");
    return 0;
}
}
