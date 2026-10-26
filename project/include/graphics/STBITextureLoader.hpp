#ifndef TEXTURE_LOADER_STBI
#define TEXTURE_LOADER_STBI

#include "graphics/Texture.hpp"
#include "graphics/TextureLoader.hpp"
#include <stb_image.h>

namespace graphics
{

class STBITextureLoader : public ITextureLoader
{
  public:
    std::shared_ptr<unsigned char> GetBytes(const char *image_path, int &width, int &height, int &nrChannels) const override;
    void LoadTexturesToRAM(const std::vector<std::string> &texturePaths) override;
    inline const std::vector<std::shared_ptr<Texture>> &GetTextures() const override
    {
        return textures;
    };
    STBITextureLoader();
  private:
    u64 idKey;
    std::vector<std::shared_ptr<Texture>> textures;
};

} // namespace graphics

#endif
