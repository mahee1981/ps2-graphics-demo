#ifndef MODEL_HPP
#define MODEL_HPP

#include <packet2.h>
#include <vector>
#include "mesh.hpp"
#include "graphics/Texture.hpp"

using namespace graphics;

class Model{
    
    using Packet2Deleter = void (*)(packet2_t*);

    private:
    std::vector<Mesh> meshList;
    std::vector<Texture> _textureList;
    // std::unique_ptr<packet2_t, Packet2Deleter> _drawBuffer;

    public:
    void LoadModel(const char* fileName, const char* materialPath);
    void Render();
    inline const std::vector<Mesh> &GetMeshList() const { return meshList; }
};

#endif
