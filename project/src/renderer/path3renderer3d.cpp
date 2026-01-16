#include "renderer/path3renderer3d.hpp"
#include "light/Lighting.hpp"
#include "logging/log.hpp"
#include "packet2.h"
#include "renderer/model.hpp"
#include "utils.hpp"
#include <algorithm>

namespace Renderer
{

Path3Renderer3D::Path3Renderer3D(int width, int height)
    : _screenWidth(width), _screenHeight(height),
      _perspectiveMatrix(
          ps2math::Mat4::perspective(Utils::ToRadians(60.0f), (float)width / (float)height, 0.1f, 2000.0f)),
      _viewPortMatrix(ps2math::Mat4::viewportTransformation(ps2math::Mat4::identity(),
                                                            width,
                                                            height,
                                                            xOff,
                                                            yOff,
                                                            float(0xFFFFFFFFu) / 16.0f)),
      context(0), trianglesRendered(0)
{
    LOG_INFO("Created perspective matrix");
    drawBuffer[0] = packet2_create(MAX_PACKET_SIZE, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
    drawBuffer[1] = packet2_create(MAX_PACKET_SIZE, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
}

void Path3Renderer3D::SetPerspectiveMatrix(float angleRadians, float aspectRatio, float near, float far)
{
    _perspectiveMatrix = ps2math::Mat4::perspective(angleRadians, aspectRatio, near, far);
}

void Path3Renderer3D::ClipVertex(ps2math::Vec4 &vertex)
{
    asm volatile("lqc2       $vf7, 0x00(%0)          \n"
                 "vclipw.xyz	$vf7, $vf7           \n" // FIXME: Clip detection is still kinda broken.
                 "vnop                               \n"
                 "vnop                               \n"
                 "vnop                               \n"
                 "vnop                               \n"
                 "cfc2       $10, $18                \n"
                 "beq        $10, $0, 3f             \n"
                 "2:                                 \n"
                 "sqc2       $0, 0x00(%0)            \n"
                 "j          4f                      \n"
                 "3:                                 \n"
                 "vdiv       $Q, $vf0w, $vf7w        \n"
                 "vwaitq                             \n"
                 "vmulq.xyz  $vf7, $vf7, $Q          \n"
                 "vmove.w    $vf7, $vf0              \n" // set w to 1.0f

                 "sqc2       $vf7, 0x00(%0)          \n"
                 "4:                                 \n"
                 "vnop                               \n"
                 :
                 : "r"(&vertex)
                 : "$10", "memory");
}

void Path3Renderer3D::RenderFrame(const std::vector<Model> &models,
                             const Light::BaseLight &mainLight,
                             const ps2math::Mat4 &viewMat)
{

    trianglesRendered = 0;
    const float timeToPrepLastDisplayList = lastDisplayListPrepWatch.GetDeltaMs();
    lastDisplayListPrepWatch.CaptureStartMoment();
    const auto viewProjMat = viewMat * _perspectiveMatrix;
    for (const Model &model : models)
    {
        // When in doubt, remember, you can get free performance by
        // not having this in the vertex loop
        ps2math::Mat4 mvp = model.GetWorldMatrix() * viewProjMat;

        for (const auto &mesh : model.GetMeshList())
        {
            auto transformedVertices = TransformMeshVertices(mesh, mvp);

            u32 qwords_needed = 1 + (mesh.Vertices.size() * 3); // 1 GIF Tag + 3 qwords per vertex
            if (qwords_needed > MAX_PACKET_SIZE)
            {
                LOG_ERROR("Mesh is too big to pack in single draw buffer, skipping");
                continue;
            }
            u32 qwords_used = packet2_get_qw_count(drawBuffer[context]);
            if (qwords_used + qwords_needed + 10 > MAX_PACKET_SIZE)
            {
                dma_wait_fast();
                dma_channel_send_packet2(drawBuffer[context], DMA_CHANNEL_GIF, 0);
                packet2_reset(drawBuffer[1 - context], false);
                context = 1 - context;
            }
            GenerateDisplayListForMesh(mesh, transformedVertices, model.GetWorldMatrix(), mainLight);
        }
    }
    packet2_update(drawBuffer[context], draw_finish(drawBuffer[context]->next));

    dma_wait_fast();
    dma_channel_send_packet2(drawBuffer[context], DMA_CHANNEL_GIF, 0);
    packet2_reset(drawBuffer[context], false);

    lastDisplayListPrepWatch.CaptureEndMoment();
    draw_wait_finish();
    graph_wait_vsync();

    if (isDebuggingEnabled)
    {
        scr_setXY(0, 0);
        scr_printf("Time to process display list: %f\n", timeToPrepLastDisplayList);
        scr_printf("Triangles sent to GS: %llu", trianglesRendered);
    }
}

inline void Path3Renderer3D::AddVertexToDisplayList(const ps2math::Vec4 &texel,
                                               const ps2math::Vec4 &vertex,
                                               const Colors::Color &lightColor,
                                               bool kickVertex = false)
{
    // This line is a reminder of how incredibly silly decisions
    // can have you lose three months of development time on PS2
    // game engines
    static constexpr texel_t zeroTexel = {.u = 1.0f, .v = 1.0f};
    texel_t texelSDK = {.u = texel.x, .v = texel.y};

    // texels
    packet2_add_u64(drawBuffer[context], texelSDK.uv);
    packet2_add_u64(drawBuffer[context], zeroTexel.uv);
    // color
    packet2_add_u64(drawBuffer[context], (u64{lightColor.b} & 0xFF) << 32 | (u64{lightColor.r} & 0xFF));
    packet2_add_u64(drawBuffer[context], (u64{0x80} & 0xFF) << 32 | (u64{lightColor.g} & 0xFF));
    // position
    packet2_add_u64(drawBuffer[context],
                    u64{Utils::FloatToFixedPoint<u16, 4>((vertex.y))} << 32 |
                        u64{Utils::FloatToFixedPoint<u16, 4>(vertex.x)});
    packet2_add_u64(drawBuffer[context],
                    u64{u32(vertex.z)} |
                        (static_cast<u64>(kickVertex & 0x01) << 48)); // on every third vertex send a drawing kick :)
}
// TODO: Benchmark REGLIST mode
void Path3Renderer3D::GenerateDisplayListForMesh(const Mesh &mesh,
                                            const std::vector<ps2math::Vec4> &transformedVertices,
                                            const ps2math::Mat4 &modelMatrix,
                                            const Light::BaseLight &mainLight)
{
    qword_t *header = drawBuffer[context]->next;
    packet2_advance_next(drawBuffer[context], sizeof(u128));
    unsigned int numberOfTimesGifTagExecutes = (mesh.Vertices.size() + 1) / 3;

    for (std::size_t i = 0; i < mesh.Vertices.size(); i += 3)
    {

        const auto &v0 = transformedVertices[i];
        const auto &v1 = transformedVertices[i + 1];
        const auto &v2 = transformedVertices[i + 2];

        if (v0.w == 0.0f || v1.w == 0.0f || v2.w == 0.0f)
        {
            --numberOfTimesGifTagExecutes;
            continue;
        }

        float triArea = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
        // Y-axis in screen space is pointing down, so the culling sign is inverted
        if (triArea > 0.0f)
        {
            --numberOfTimesGifTagExecutes;
            continue;
        }

        const auto &t0 = mesh.Texels[i];
        const auto &t1 = mesh.Texels[i + 1];
        const auto &t2 = mesh.Texels[i + 2];

        const auto &lightColor0 = 
            Light::CalculateLightingRGBA8((mesh.Normals[i] * modelMatrix).Normalize(), mainLight);
        const auto &lightColor1 =
            Light::CalculateLightingRGBA8((mesh.Normals[i + 1] * modelMatrix).Normalize(), mainLight);
        const auto &lightColor2 =
            Light::CalculateLightingRGBA8((mesh.Normals[i + 2] * modelMatrix).Normalize(), mainLight);

        AddVertexToDisplayList(t0, v0, lightColor0);
        AddVertexToDisplayList(t1, v1, lightColor1);
        AddVertexToDisplayList(t2, v2, lightColor2, true);

        trianglesRendered++;
    }
    // PRIM REG = 0x5B -> triangle with Gouraud shading, texturing, alpha blending
    constexpr prim_t primitiveTypeConfig{.type = PRIM_TRIANGLE,
                                         .shading = PRIM_SHADE_GOURAUD,
                                         .mapping = DRAW_ENABLE,
                                         .fogging = DRAW_DISABLE,
                                         .blending = DRAW_ENABLE,
                                         .antialiasing = DRAW_DISABLE,
                                         .mapping_type = PRIM_MAP_ST,
                                         .colorfix = PRIM_UNFIXED};
    constexpr u64 primRegValue = Utils::GetPrimGSValue(primitiveTypeConfig);

    header->dw[0] = (u64)GIF_SET_TAG(3 * numberOfTimesGifTagExecutes, false, true, primRegValue, GIF_FLG_PACKED, 3);

    constexpr u64 triangleGIFTag = u64(GIF_REG_XYZ2) << 8 | u64(GIF_REG_RGBAQ) << 4 | u64(GIF_REG_ST);
    header->dw[1] = triangleGIFTag;
}
std::vector<ps2math::Vec4> Renderer::Path3Renderer3D::TransformMeshVertices(const Mesh &mesh, const ps2math::Mat4 &mvp)
{

    std::vector<ps2math::Vec4> transformedVertices;
    transformedVertices.reserve(mesh.Vertices.size());

    const auto& viewPortMat = _viewPortMatrix;

    std::transform(mesh.Vertices.begin(),
                   mesh.Vertices.end(),
                   std::back_inserter(transformedVertices),
                   [&mvp, &viewPortMat](ps2math::Vec4 vertex) {
                       vertex = vertex * mvp;
                       // clip vertices and perform perspective division
                       Path3Renderer3D::ClipVertex(vertex);

                       if (vertex.x == 0.0f && vertex.y == 0.0f && vertex.z == 0.0f)
                           return ps2math::Vec4(0.0f, 0.0f, 0.0f, 0.0f);

                       return vertex * viewPortMat;
                   });
    return transformedVertices;
}
} // namespace Renderer
