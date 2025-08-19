#ifndef TEXTURE_LOADER_STBI
#define TEXTURE_LOADER_STBI

#include <stb_image.h>
#include "graphics/TextureLoader.hpp"

namespace graphics {

class STBITextureLoader : public ITextureLoader {
public:
    std::shared_ptr<unsigned char> GetBytes(const char * image_path, int& width, int& height, int& nrChannels) override;
};

}

#endif
