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
#include "graphics/GSTextureBuffer.hpp"
#include "graphics/TextureLoader.hpp"

namespace graphics
{

class TextureCache;

/// A texture in EE emory that may or may not currently have a home in GS
/// local memory
///
/// The texture does NOT own its VRAM. A TextureCache hands it an address and
/// can take it away again, so vramAddress is only meaningful while
/// IsResident() is true. Everything the cache needs in order to make that
/// decision is exposed here: the size it would occupy, whether its pixels are
/// currently visible in VRAM, and how to push them there.
class Texture
{
  public:
    Texture(const char *pathToImg, Buffers::GSPixelStorageMethod psm = Buffers::PSM_24);
    Texture(std::string pathToImg, Buffers::GSPixelStorageMethod psm = Buffers::PSM_24);
    ~Texture();
    void LoadTexture(std::shared_ptr<ITextureLoader> textureLoader);
    /// Deprecated, legacy
    int AllocateVram();

    /// Uploads the pixel data to the current VRAM address over PATH3 and marks
    /// the texture as resident. Blocks until the transfer completes.
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
    u64 GetTexGSSettings() const;

    

    inline Buffers::GSPixelStorageMethod GetPixelStorageMethod() const
    {
        return pixelStorageMethod;
    }

    inline bool HasPixelData() const
    {
        return imageData != nullptr;
    }

    inline u32 GetVramAddres() const
    {
        return vramAddress;
    }

    /// True when texture is resident in VRAM
    inline bool IsResident() const
    {
        return resident;
    }
    /// Footprint in 32-bit words, block aligned, as VramManager counts it.
    /// when palleted formats are added, the CLUT should be folded in here so
    /// texture and palette stay a single allocation with a single lifetime
    u32 GetVramSizeWords() const;

    /// Called by TextureCache once space has been reserved. Clears residency,
    /// so the next bind uploads to the new address.
    void SetVramAddress(u32 addressWords, TextureCache* owner);
    
    /// Called by TextureCache once space has been reserved. Clears residency,
    /// so the next bind uploads to the new address
    void OnEvicted();
  private:

    /// Bits each pixel occupies during a host to local transfer. Differs from
    /// the VRAM footprint: PSM_24 is transferred as packed 24-bit pixels but
    /// stored one word per pixel
    u32 GetTransferBitsPerPixel() const;

    /// Qwords the pixel data occupies in the GIF, derived from the PSM rather
    /// than assumed to be 4 bytes per pixel
    u32 GetTransferQwordCount() const;

    std::string imgPath;
    std::shared_ptr<unsigned char> imageData;
    int width, height, nrChannels;
    Buffers::GSPixelStorageMethod pixelStorageMethod;
    unsigned int vramAddress;
    bool resident;
    TextureCache *residencyOwner;
    std::shared_ptr<ITextureLoader> textureLoader;
};
} // namespace graphics
#endif
