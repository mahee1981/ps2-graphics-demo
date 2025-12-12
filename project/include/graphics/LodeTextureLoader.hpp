#ifndef TEXTURE_LOADER_LODE
#define TEXTURE_LOADER_LODE

#include "graphics/TextureLoader.hpp"
#include <lodepng.h>

namespace graphics
{

class LODETextureLoader : public ITextureLoader
{
  public:
    std::shared_ptr<unsigned char> GetBytes(const char *image_path, int &width, int &height, int &nrChannels) override;
};

} // namespace graphics

#endif
