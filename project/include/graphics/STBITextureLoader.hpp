#ifndef TEXTURE_LOADER_STBI
#define TEXTURE_LOADER_STBI

#include "graphics/Texture.hpp"
#include "graphics/TextureLoader.hpp"
#include <stb_image.h>
#include <list>

namespace graphics
{

class STBITextureLoader : public ITextureLoader
{
  public:
    std::shared_ptr<unsigned char> GetBytes(const char *image_path, int &width, int &height, int &nrChannels) const override;
    std::list<std::shared_ptr<Texture>> LoadTexturesToRAM(const std::vector<std::string> &texturePaths) override;
    STBITextureLoader();
  private:
    u64 idKey;
};

} // namespace graphics

#endif
