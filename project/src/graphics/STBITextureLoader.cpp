#include "graphics/STBITextureLoader.hpp"

namespace graphics
{

STBITextureLoader::STBITextureLoader() : idKey(0) {}

std::shared_ptr<unsigned char> STBITextureLoader::GetBytes(const char *image_path,
                                                           int &width,
                                                           int &height,
                                                           int &nrChannels) const
{
    stbi_set_flip_vertically_on_load(true);
    return {stbi_load(image_path, &width, &height, &nrChannels, 3), [](unsigned char *data) { stbi_image_free(data); }};
}

std::list<std::shared_ptr<Texture>> STBITextureLoader::LoadTexturesToRAM(const std::vector<std::string> &texturePaths)
{
    std::list<std::shared_ptr<Texture>> loadedTextures;

    for (auto &&texturePath : texturePaths)
    {
        auto currTexture = std::make_shared<graphics::Texture>(texturePath, idKey++);

        currTexture->LoadTexture(this);
        loadedTextures.push_back(currTexture);
    }

    return loadedTextures;
}

} // namespace graphics
