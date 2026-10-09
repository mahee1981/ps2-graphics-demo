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

#include "graphics/GSBufferConfig.hpp"
#include "graphics/TextureLoader.hpp"

namespace graphics
{

class TextureCache;

class Texture
{
  public:
    Texture(const char *pathToImg);
    Texture(std::string pathToImg);
    ~Texture();
    void LoadTexture(std::shared_ptr<ITextureLoader> textureLoader);
    // using legacy PS2SDK allocator
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
    void SetTexSamplingMethodInGS();
    u64 GetTexGSSettings() const;

    inline unsigned int GetVramAddress() const
    {
        return vramAddress;
    }
    void SetVramAddress(u32 vramAddress);

    // This is the size of the texture in number of 32 bit words
    // Since PSM_24 is actually RGBA -> 32 Bits -> 1 pixel, then
    // this is width * height
    // TODO: fix for other texture types
    u32 GetVramSizeWords() const;

    inline bool IsResident() const { return resident; }
    inline bool HasPixelData() const { return imageData != nullptr; }

    void OnEvicted();

    inline void SetResidencyOwner(TextureCache *owner)
    {
        residencyOwner = owner;
    }

  private:
    std::string imgPath;
    std::shared_ptr<unsigned char> imageData;
    int width, height, nrChannels;
    unsigned int vramAddress;
    Buffers::GSPixelStorageMethod pixelStorageMethod;
    bool resident;
    TextureCache* residencyOwner;

    std::shared_ptr<ITextureLoader> textureLoader;
};
} // namespace graphics
#endif
