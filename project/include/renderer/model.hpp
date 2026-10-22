#ifndef MODEL_HPP
#define MODEL_HPP

#include <packet2.h>

#include <memory>
#include <unordered_map>
#include <vector>

#include "VU0Math/mat4.hpp"
#include "VU0Math/vec4.hpp"
#include "components/transform.hpp"
#include "graphics/Texture.hpp"
#include "mesh.hpp"
#include "renderer/material.hpp"

namespace Renderer
{


using namespace graphics;
class Model
{

    using Packet2Deleter = void (*)(packet2_t *);

  public:
    Model(const ps2math::Vec4 &position);
    void LoadModel(const char *fileName, const char *materialPath = nullptr);
    void LoadModelPackets(const char *fileName);
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

    inline const std::vector<MaterialMesh> &GetMeshList() const
    {
        return meshList;
    }

    // TODO: add bounds checking maybe?
    inline const std::shared_ptr<Texture> &GetTextureById(int id) const
    {
        return _textureList.at(id);
    }

    inline const Material &GetMaterialById(int id) const
    {
        // TODO: materail ID might be -1 so need to handle that with default
        return materialLUT[id];

    }

    inline void AddTexture(std::shared_ptr<Texture> texPtr)
    {
        _textureList.push_back(texPtr);
    }

  private:
    std::vector<MaterialMesh> meshList;
    std::vector<std::shared_ptr<Texture>> _textureList;
    ps2math::Mat4 _worldMatrix;
    Components::Transform _transformComponent;
    using MaterialIndex = int;
    std::vector<Material> materialLUT;


    using NumberOfVertices = std::size_t;
    std::unordered_map<MaterialIndex, NumberOfVertices> CountVerticesPerMaterial(const std::vector<int> &materialIds);
};
} // namespace Renderer
#endif
