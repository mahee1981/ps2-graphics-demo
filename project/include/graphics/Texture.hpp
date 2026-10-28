#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <dma.h>
#include <dma_tags.h>
#include <draw.h>
#include <graph.h>
#include <gs_gp.h>
#include <packet2.h>

#include <memory>

#include "gs/buffers/GSBufferConfig.hpp"

namespace graphics
{

class Texture
{
  public:
    Texture(u32 id, std::shared_ptr<u8> bytes, u32 width, u32 height, u32 nrChannels);
    void TransferTextureToGS();
    inline u32 GetWidth() const
    {
        return width;
    }
    inline u32 GetHeight() const
    {
        return height;
    }
    inline u32 GetNrChannels() const
    {
        return nrChannels;
    }
    inline u32 GetId() const
    {
        return textureID;
    }
    static void SetTexSamplingMethodInGS();
    u64 GetTexGSSettings(u32 vramAddress) const;

    // This is the size of the texture in number of 32 bit words
    // Since PSM_24 is actually RGBA -> 32 Bits -> 1 pixel, then
    // this is width * height
    // TODO: fix for other texture types
    u32 GetVramSizeWords() const;

    inline bool HasPixelData() const { return imageData != nullptr; }

    std::shared_ptr<packet2_t> GenerateUploadBuffer(u32 vramAddres, bool partOfCallChain = false) const;

  private:
    std::shared_ptr<unsigned char> imageData;
    int width, height, nrChannels;
    GS::Buffers::GSPixelStorageMethod pixelStorageMethod;
    u32 textureID;


};
} // namespace graphics
#endif
