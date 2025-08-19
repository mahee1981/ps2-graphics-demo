#include "graphics/texture.hpp"

namespace graphics
{

    Texture::Texture(const char *pathToImg, std::shared_ptr<ITextureLoader> textureLoader)
        : imgPath(pathToImg), imageData(nullptr), width(0), height(0), nrChannels(0), gsTextureBuffer(nullptr), textureLoader(textureLoader)
    {
    }

    Texture::Texture(std::string pathToImg, std::shared_ptr<ITextureLoader> textureLoader)
            : imgPath(pathToImg), imageData(nullptr), width(0), height(0), nrChannels(0), gsTextureBuffer(nullptr), textureLoader(textureLoader)
    {
    }

    Texture::~Texture()
    {
    }

    int Texture::AllocateVram()
    {
        gsTextureBuffer = std::make_unique<Buffers::GSTextureBuffer>(this->width, this->height, Buffers::PSM_24);
        gsTextureBuffer->AllocateVRAMForBuffer();
        return this->gsTextureBuffer->GetVramAddress();
    }

    void Texture::TransferTextureToGS()
    {
        u64 destBufBasePtr = (this->gsTextureBuffer->GetVramAddress() >> 6) & 0x3FFF;
        u64 destBufWidth = (this->width >> 6) & 0x3F;
        u64 psm = (Buffers::GSPixelStorageMethod::PSM_24) & 0x3F;

        // BITBLTBUF - setting for transmission between the buffers
        // Transfering from EE to GS does not require a source buffer information
        u64 gsBitBltBufValue = (destBufBasePtr << 32) | (destBufWidth << 48) | (psm << 56);

        // TRXPOS - specification of transmission areas in buffers
        //  (i.e. start coordinates of image and transmission direction)

        constexpr u64 xCoordinateSrc = 0 & 0x7FF;
        constexpr u64 yCoordinateSrc = 0 & 0x7FF;
        constexpr u64 xCoordinateDest = 0 & 0x7FF;
        constexpr u64 yCoordinateDest = 0 & 0x7FF;

        //  Upper Left -> Lower Right  00
        //  Lower Left -> Upper Right  01
        //  Upper Right -> Lower Left  10
        //  Lower Right -> Upper Left  11
        constexpr u64 direction = 0 & 0x03;

        constexpr u64 trxPosValue = (xCoordinateSrc) | (yCoordinateSrc << 16) | (xCoordinateDest << 32) | (yCoordinateDest) << 48 | (direction << 59);

        // TRXREG - width and heigh of texture to be transfered
        u64 trxRegValue = u64(this->width & 0xFFF) | (u64(this->height & 0xFFF) << 32);

        // TRXDIR - direction of the transmission
        //  00  Host -> Local Transmission
        //  01  Local -> Host Transmission
        //  10  Local -> Local Transmission
        //  11  Transmission is deactivated.

        constexpr u64 transmissionDirection = 0;


        // we need to split the transfer in max NLOOP size which is a 15bit value
        // then also transfer what's left
        constexpr u64 MAX_TRANSFER_SIZE = 0x7FFF; //
        u64 qwordCount = (width * height) >> 2;   // (width*height*32bits)/128bits = (width*height)/4;
        int numberOfWholeGifBlockIterations = qwordCount / MAX_TRANSFER_SIZE;
        int remainingData = qwordCount % MAX_TRANSFER_SIZE;

        // Transfer stuff
        using Packet2Deleter = void (*)(packet2_t *);
        std::unique_ptr<packet2_t, Packet2Deleter> chain_packet{packet2_create(100, P2_TYPE_NORMAL, P2_MODE_CHAIN, 1),
                                                                [](packet2_t *packet)
                                                                { packet2_free(packet); }};

        // Setup the transfer
        qword_t qword;
        DMATAG_CNT((&qword), 5, 0, 0, 0); // macro magic //will explore DMA Chain Tags later ž
        packet2_add_u128(chain_packet.get(), qword.qw);

        qword.dw[0] = u64(GIF_SET_TAG(4, 0, 0, 0, GIF_FLG_PACKED, 1));
        qword.dw[1] = u64(GIF_REG_AD);
        packet2_add_u128(chain_packet.get(), qword.qw);

        qword.dw[0] = gsBitBltBufValue;
        qword.dw[1] = GS_REG_BITBLTBUF;
        packet2_add_u128(chain_packet.get(), qword.qw);

        qword.dw[0] = trxPosValue;
        qword.dw[1] = GS_REG_TRXPOS;
        packet2_add_u128(chain_packet.get(), qword.qw);

        qword.dw[0] = trxRegValue;
        qword.dw[1] = GS_REG_TRXREG;
        packet2_add_u128(chain_packet.get(), qword.qw);

        qword.dw[0] = transmissionDirection;
        qword.dw[1] = GS_REG_TRXDIR;
        packet2_add_u128(chain_packet.get(), qword.qw);

        if(imageData.get())
            printf("Good image data");

        void *src = (void*)imageData.get();

        // now transfer the actual image bytes in MaxTransferSize if any whole blocks exit
        while (numberOfWholeGifBlockIterations-- > 0)
        {
            // Continue tag to set the dmac to image mode
            DMATAG_CNT((&qword), 1, 0, 0, 0);
            packet2_add_u128(chain_packet.get(), qword.qw);

            // Set DMAC to image mode
            qword.dw[0] = u64(GIF_SET_TAG(GIF_BLOCK_SIZE, 0, 0, 0, GIF_FLG_IMAGE, 0));
            qword.dw[1] = 0;
            packet2_add_u128(chain_packet.get(), qword.qw);

            // Transfer from actual image data via REF tag
            DMATAG_REF((&qword), GIF_BLOCK_SIZE, (unsigned int)src, 0, 0, 0);
            packet2_add_u128(chain_packet.get(), qword.qw);

            // Now increment the address by the number of qwords in bytes
            src = (void *)((u8 *)src + (GIF_BLOCK_SIZE * 16));
        }


        // same as above, just remaining data
        if (remainingData)
        {
            // Continue tag to set the dmac to image mode
            DMATAG_CNT((&qword), 1, 0, 0, 0);
            packet2_add_u128(chain_packet.get(), qword.qw);

            // Set DMAC to image mode
            qword.dw[0] = u64(GIF_SET_TAG(remainingData, 0, 0, 0, GIF_FLG_IMAGE, 0));
            qword.dw[1] = 0;
            packet2_add_u128(chain_packet.get(), qword.qw);

            // Transfer from actual image data via REF tag
            // REF transfers from actual memory address instead of a buffer that we prepare
            DMATAG_REF((&qword), remainingData, (unsigned int)src, 0, 0, 0);
            packet2_add_u128(chain_packet.get(), qword.qw);
        }
        packet2_update(chain_packet.get(), draw_texture_flush(chain_packet->next));
        dma_channel_send_packet2(chain_packet.get(), DMA_CHANNEL_GIF, 1);

        dma_wait_fast();
    }

    void Texture::SetTextureAsActive()
    {
        using Packet2Deleter = void (*)(packet2_t *);
        std::unique_ptr<packet2_t, Packet2Deleter> normal_packet{packet2_create(10, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0),
                                                                [](packet2_t *packet)
                                                                { packet2_free(packet); }};

        qword_t qword; 

        qword.dw[0] = u64(GIF_SET_TAG(1, 0, 0, 0, GIF_FLG_PACKED, 1));
        qword.dw[1] = u64(GIF_REG_AD);
        packet2_add_u128(normal_packet.get(), qword.qw);

        qword.dw[0] = ((u64(this->gsTextureBuffer->GetVramAddress()) >> 6)  & 0x3fff) |
                      (((u64(width) >> 6) & 0x3f) << 14) |
                      ((u64(Buffers::GSPixelStorageMethod::PSM_24) & 0x3f) << 20) |
                      ((u64(draw_log2(width)) & 0xf) << 26) |
                      ((u64(draw_log2(height)) & 0xf) << 30) |
                      ((u64(0) & 0x1) << 34) | // RGB or RGBA
                      ((u64(0) & 0x3) << 35) | 0; // 0 -> Modulate, 1 -> Decal
        qword.dw[1] = GS_REG_TEX0_1;
        packet2_add_u128(normal_packet.get(), qword.qw);
        
        dma_channel_send_packet2(normal_packet.get(), DMA_CHANNEL_GIF, 0);
        dma_wait_fast();
    }

    // FOR NOW HARDCODED
    void Texture::SetTexSamplingMethodInGS()
    {
        using Packet2Deleter = void (*)(packet2_t *);
        std::unique_ptr<packet2_t, Packet2Deleter> normal_packet{packet2_create(15, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0),
                                                                [](packet2_t *packet)
                                                                { packet2_free(packet); }};
        lod_t lod;

        lod.calculation = LOD_USE_K;
        lod.max_level = 0;
        lod.mag_filter = LOD_MAG_NEAREST;
        lod.min_filter = LOD_MIN_NEAREST;
        lod.l = 0;
        lod.k = 0;

        packet2_update(normal_packet.get(), draw_texture_sampling(normal_packet->next, 0, &lod));

        dma_channel_send_packet2(normal_packet.get(), DMA_CHANNEL_GIF, 0);
        dma_wait_fast();

    }



    void Texture::LoadTexture()
    {
        imageData = textureLoader->GetBytes(imgPath.c_str(), width, height, nrChannels);
    }

}
