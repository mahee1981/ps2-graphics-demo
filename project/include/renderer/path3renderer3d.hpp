#ifndef RENDERER_3D_HPP
#define RENDERER_3D_HPP

#include "VU0Math/mat4.hpp"
#include "common/Color.hpp"
#include "interfaces/IRenderer3D.hpp"
#include "light/BaseLight.hpp"
#include "packet2_types.h"
#include "renderer/model.hpp"
#include "tools/Deltawatch.hpp"
#include <vector>

namespace Renderer
{

class Path3Renderer3D : public IRenderer3D
{
  public:
    Path3Renderer3D(int width, int height);
    void SetPerspectiveMatrix(float angleRadians, float aspectRatio, float near, float far) override;
    const ps2math::Mat4 &GetPerspectiveMatrix() const override
    {
        return _perspectiveMatrix;
    }
    const ps2math::Mat4 &GetViewPortMatrix() const
    {
        return _viewPortMatrix;
    }
    static void ClipVertex(ps2math::Vec4 &vertex);
    void RenderFrame(const std::vector<Model> &models,
                     const Light::BaseLight &mainLight,
                     const ps2math::Mat4 &viewProjMatrix) override;
    void ToggleDebugPrint() override
    {
        isDebuggingEnabled = !isDebuggingEnabled;
    };

  private:
    float _screenWidth;
    float _screenHeight;
    ps2math::Mat4 _perspectiveMatrix;
    ps2math::Mat4 _viewPortMatrix;
    // TODO: need to make transfers dynamic and split it into slices if bigger than u16_max
    // OPTIONAL: make packed DMA transfers
    std::array<packet2_t *, 2> drawBuffer;
    u32 context;
    bool isDebuggingEnabled;
    Deltawatch lastDisplayListPrepWatch;
    long long unsigned int trianglesRendered;

    void GenerateDisplayListForMesh(const Mesh &mesh,
                                    const std::vector<ps2math::Vec4> &transformedVertices,
                                    const ps2math::Mat4 &modelMatrix,
                                    const Light::BaseLight &mainLight);
    void AddVertexToDisplayList(const ps2math::Vec4 &texel,
                                const ps2math::Vec4 &vertex,
                                const Colors::Color &lightColor,
                                bool kickVertex);
    std::vector<ps2math::Vec4> TransformMeshVertices(const Mesh &mesh, const ps2math::Mat4 &mvp);
};

} // namespace Renderer

#endif
