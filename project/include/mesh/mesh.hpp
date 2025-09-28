#ifndef MESH_HPP
#define MESH_HPP

#include <vector>
#include "VU0Math/vec4.hpp"
#include "graphics/Texture.hpp"
#include <debug.h>
#include <draw_types.h>
#include "tiny_obj_loader.h"

using namespace graphics;

class Mesh{

    public:
    std::vector<ps2math::Vec4> _vertexPositionCoord;
    std::vector<ps2math::Vec4> _vertexNormalCoord;
    std::vector<texel_t> _textureCoordinates;
    std::vector<int> _vertexIndices;
    std::vector<int> _normalIndices;
    std::vector<int> _textureIndices;

};




#endif