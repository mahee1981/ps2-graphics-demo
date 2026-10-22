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
    return {stbi_load(image_path, &width, &height, &nrChannels, 3), &stbi_image_free};
}

void STBITextureLoader::LoadTexturesToRAM(const std::vector<std::string> &texturePaths)
{
    for (auto &&texturePath : texturePaths)
    {
        int width, height,nrChannels;
        std::shared_ptr<u8> bytes = GetBytes(texturePath.c_str(), width, height, nrChannels);
        auto currTexture = std::make_shared<graphics::Texture>(idKey++,
                                                               bytes,
                                                               static_cast<u32>(width),
                                                               static_cast<u32>(height),
                                                               static_cast<u32>(nrChannels));

        textures.push_back(currTexture);
    }
}

} // namespace graphics
