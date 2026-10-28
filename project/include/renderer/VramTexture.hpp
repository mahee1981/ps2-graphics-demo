#ifndef VRAM_TEXTURE_HPP
#define VRAM_TEXTURE_HPP
#include "graphics/Texture.hpp"

namespace Renderer
{

class VramTexture
{
    std::shared_ptr<const graphics::Texture> texturePtr;
    std::shared_ptr<packet2_t> uploadBuffer;
    u32 vramAddress;
    bool resident;

  public:
    VramTexture(const std::shared_ptr<graphics::Texture> texturePtr)
        : texturePtr(texturePtr), uploadBuffer(nullptr), vramAddress(0), resident(false)
    {
    }
    void SetVramAddress(u32 vramAddress);

    inline std::shared_ptr<packet2_t> GetDMAPacket() const 
    {
        return uploadBuffer;
    }
    inline unsigned int GetVramAddress() const
    {
        return vramAddress;
    }
    inline bool IsResident() const
    {
        return resident;
    }
    inline void SetResident()
    {
        this->resident = true;
    }
    inline u32 GetTextureDataId() const
    {
        return texturePtr->GetId();
    }

    inline u64 GetTextureActiveSettings() const
    {
        return texturePtr->GetTexGSSettings(vramAddress);
    }

    // also hardcoded for now
    inline u32 GetVramSizeWords() const
    {
        return texturePtr->GetWidth() * texturePtr->GetHeight();
    }

    void OnEvicted();
};
}
#endif

