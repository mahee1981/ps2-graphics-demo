#include "graphics/LodeTextureLoader.hpp"
#include <cstring>
#include <vector>

namespace graphics
{

LODETextureLoader::LODETextureLoader() : idKey(0)
{
}

std::shared_ptr<unsigned char> LODETextureLoader::GetBytes(const char *image_path,
                                                           int &width,
                                                           int &height,
                                                           int &nrChannels) const
{
    std::vector<unsigned char> tempImgData;
    unsigned int Uwidth, Uheight;
    lodepng::decode(tempImgData, Uwidth, Uheight, image_path);

    width = Uwidth;
    height = Uheight;
    auto ptr =
        std::shared_ptr<unsigned char>(new unsigned char[tempImgData.size()], std::default_delete<unsigned char[]>());
    std::memcpy(ptr.get(), tempImgData.data(), tempImgData.size());
    return ptr;
}

void LODETextureLoader::LoadTexturesToRAM(const std::vector<std::string> &texturePaths)
{
    for (auto &&texturePath : texturePaths)
    {
        int width, height, nrChannels;
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
