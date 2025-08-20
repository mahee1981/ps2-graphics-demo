#ifndef TEXTURE_LOADER_ITF
#define TEXTURE_LOADER_ITF

#include <memory>

namespace graphics {

class ITextureLoader {
public:
    virtual std::shared_ptr<unsigned char> GetBytes(const char * image_path, int& width, int& height, int& nrChannels) = 0;
};

}

#endif
