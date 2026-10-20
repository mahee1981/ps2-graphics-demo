#include "graphics/LodeTextureLoader.hpp"
#include <cstring>
#include <vector>

namespace graphics
{

LODETextureLoader::LODETextureLoader() : idKey(0) {}

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


std::list<std::shared_ptr<Texture>> LODETextureLoader::LoadTexturesToRAM(const std::vector<std::string> &texturePaths)
{
    std::list<std::shared_ptr<Texture>> loadedTextures;

    for (auto &&texturePath : texturePaths)
    {
        auto currTexture = std::make_shared<graphics::Texture>(texturePath, idKey++);

        currTexture->LoadTexture(this);
        currTexture->AllocateVram();
        currTexture->TransferTextureToGS();
        loadedTextures.push_back(currTexture);
    }

    return loadedTextures;
}

} // namespace graphics
