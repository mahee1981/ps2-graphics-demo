#ifndef MODEL_HPP
#define MODEL_HPP

#include "VU0Math/mat4.hpp"
#include "VU0Math/vec4.hpp"
#include "components/transform.hpp"
#include "graphics/Texture.hpp"
#include "mesh.hpp"
#include <packet2.h>
#include <vector>

namespace Renderer
{

using namespace graphics;
class Model
{

    using Packet2Deleter = void (*)(packet2_t *);

  public:
    Model(const ps2math::Vec4 &position);
    void LoadModel(const char *fileName, const char *materialPath = nullptr);
    void Render();
    void Update();

    std::vector<ps2math::Vec4> TransformVertices(const ps2math::Mat4 &mvp,
                                                 float width,
                                                 float height,
                                                 float xOff,
                                                 float yOff);

    inline Components::Transform &GetTransformComponent()
    {
        return _transformComponent;
    }

    inline const ps2math::Mat4 &GetWorldMatrix() const
    {
        return _worldMatrix;
    }

    inline const std::vector<Mesh> &GetMeshList() const
    {
        return meshList;
    }



  private:
    std::vector<Mesh> meshList;
    std::vector<Texture> _textureList;
    ps2math::Mat4 _worldMatrix;
    Components::Transform _transformComponent;
};
} // namespace Renderer
#endif
