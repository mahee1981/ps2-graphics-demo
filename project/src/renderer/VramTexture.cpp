#include "renderer/VramTexture.hpp"

namespace Renderer
{

    void VramTexture::SetVramAddress(u32 vramAddress)
    {
        this->vramAddress = vramAddress;
        uploadBuffer = texturePtr->GenerateUploadBuffer(true);
        resident = false;
    }
    void VramTexture::OnEvicted()
    {
        vramAddress = 0;
        resident = false;
        uploadBuffer = nullptr;
    }
}
