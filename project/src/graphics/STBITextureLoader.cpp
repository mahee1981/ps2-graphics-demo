#include "graphics/STBITextureLoader.hpp"

namespace graphics {


    std::shared_ptr<unsigned char> STBITextureLoader::GetBytes(const char * image_path, int& width, int& height, int& nrChannels) 
    {
        
      return {stbi_load(image_path, &width, &height, &nrChannels, 0),
                     [](unsigned char *data)
                     { stbi_image_free(data); printf("Deleted image!\n"); }};
    }


}
