#ifndef TEXTURE_LOADER_STBI
#define TEXTURE_LOADER_STBI

#include "graphics/TextureLoader.hpp"
#include <stb_image.h>

namespace graphics
{

class STBITextureLoader : public ITextureLoader
{
  public:
    std::shared_ptr<unsigned char> GetBytes(const char *image_path, int &width, int &height, int &nrChannels) override;
};

} // namespace graphics

#endif
