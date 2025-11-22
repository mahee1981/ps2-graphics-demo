#ifndef MESH_HPP
#define MESH_HPP

#include <vector>
#include "VU0Math/vec4.hpp"
#include <debug.h>
#include <draw_types.h>
#include "tiny_obj_loader.h"


class Mesh{

    public:
    std::vector<int> VertexIndices;
    std::vector<int> _normalIndices;
    std::vector<int> TexIndices;

};




#endif
