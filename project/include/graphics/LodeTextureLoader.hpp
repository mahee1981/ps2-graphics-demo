#ifndef TEXTURE_LOADER_LODE
#define TEXTURE_LOADER_LODE

#include "graphics/TextureLoader.hpp"
#include "tamtypes.h"
#include <lodepng.h>
#include "graphics/Texture.hpp"

namespace graphics
{


class LODETextureLoader : public ITextureLoader
{
  public:
    std::shared_ptr<unsigned char> GetBytes(const char *image_path, int &width, int &height, int &nrChannels) const override;
    void LoadTexturesToRAM(const std::vector<std::string> &texturePaths) override;
    inline const std::vector<std::shared_ptr<Texture>> &GetTextures() const override
    {
        return textures;
    };
    LODETextureLoader();
  private:
    u64 idKey;
    std::vector<std::shared_ptr<Texture>> textures;
};

} // namespace graphics

#endif
