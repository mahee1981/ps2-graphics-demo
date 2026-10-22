#ifndef TEXTURE_LOADER_ITF
#define TEXTURE_LOADER_ITF


#include "tamtypes.h"
#include <memory>
#include <vector>
#include <string>



namespace graphics
{

class Texture;

class ITextureLoader
{
  public:
    virtual std::shared_ptr<u8> GetBytes(const char *image_path,
                                                    int &width,
                                                    int &height,
                                                    int &nrChannels) const = 0;
    virtual void LoadTexturesToRAM(const std::vector<std::string> &texturePaths) = 0;
    virtual const std::vector<std::shared_ptr<Texture>> &GetTextures() const = 0;
};

} // namespace graphics

#endif
