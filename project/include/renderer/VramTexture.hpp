#ifndef VRAM_TEXTURE_HPP
#define VRAM_TEXTURE_HPP
#include "graphics/Texture.hpp"

namespace Renderer
{

class VramTexture
{
    std::shared_ptr<const graphics::Texture> texturePtr;
    u32 vramAddress;
    bool resident;
    std::shared_ptr<packet2_t> uploadBuffer;

  public:
    VramTexture(const std::shared_ptr<graphics::Texture> texturePtr)
        : texturePtr(texturePtr), vramAddress(0), resident(false), uploadBuffer(nullptr)
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

    // also hardcoded for now
    inline u32 GetVramSizeWords() const
    {
        return texturePtr->GetWidth() * texturePtr->GetHeight();
    }

    void OnEvicted();
};
}
#endif

