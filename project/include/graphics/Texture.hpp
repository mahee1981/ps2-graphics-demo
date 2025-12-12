#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <dma.h>
#include <dma_tags.h>
#include <draw.h>
#include <graph.h>
#include <gs_gp.h>
#include <packet2.h>

#include <memory>
#include <string>

#include "graphics/GSTextureBuffer.hpp"
#include "graphics/TextureLoader.hpp"

namespace graphics
{
class Texture
{
  public:
    Texture(const char *pathToImg);
    Texture(std::string pathToImg);
    ~Texture();
    void LoadTexture(std::shared_ptr<ITextureLoader> textureLoader);
    int AllocateVram();
    void TransferTextureToGS();
    inline int GetWidth() const
    {
        return width;
    }
    inline int GetHeight() const
    {
        return height;
    }
    inline int GetNrChannels() const
    {
        return nrChannels;
    }
    void SetTextureAsActive();
    void SetTexSamplingMethodInGS();

  private:
    std::string imgPath;
    std::shared_ptr<unsigned char> imageData;
    int width, height, nrChannels;
    std::unique_ptr<Buffers::GSTextureBuffer> gsTextureBuffer;
    std::shared_ptr<ITextureLoader> textureLoader;
};
} // namespace graphics
#endif
