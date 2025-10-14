#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include <graph.h> 
#include <draw.h>
#include <packet2.h>
#include <gs_gp.h>
#include <dma_tags.h>
#include <dma.h>

#include <string>
#include <memory>

#include "graphics/TextureLoader.hpp"
#include "graphics/GSTextureBuffer.hpp"
#include "graphics/TextureLoader.hpp"


namespace graphics {
    class Texture
    {
        public:
            Texture(const char* pathToImg);
            Texture(std::string pathToImg);
            ~Texture();
            void LoadTexture(std::shared_ptr<ITextureLoader> textureLoader);
            int AllocateVram();
            void TransferTextureToGS();
            inline int GetWidth() const { return width; } 
            inline int GetHeight() const { return height; } 
            inline int GetNrChannels() const { return nrChannels; } 
            void SetTextureAsActive();
            void SetTexSamplingMethodInGS();

        private:
            std::string imgPath;
            std::shared_ptr<unsigned char> imageData;
            int width, height, nrChannels;
            std::unique_ptr<Buffers::GSTextureBuffer> gsTextureBuffer;
            std::shared_ptr<ITextureLoader> textureLoader;
            

    };
}
#endif
