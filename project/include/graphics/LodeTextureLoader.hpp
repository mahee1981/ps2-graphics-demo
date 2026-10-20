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
    std::list<std::shared_ptr<Texture>> LoadTexturesToRAM(const std::vector<std::string> &texturePaths) override;
    LODETextureLoader();
  private:
    u64 idKey;
};

} // namespace graphics

#endif
