#ifndef TEXTURE_LOADER_ITF
#define TEXTURE_LOADER_ITF


#include <memory>
#include <vector>
#include <list>
#include <string>



namespace graphics
{

class Texture;

class ITextureLoader
{
  public:
    virtual std::shared_ptr<unsigned char> GetBytes(const char *image_path,
                                                    int &width,
                                                    int &height,
                                                    int &nrChannels) const = 0;
    virtual std::list<std::shared_ptr<Texture>> LoadTexturesToRAM(const std::vector<std::string> &texturePaths) = 0;
};

} // namespace graphics

#endif
