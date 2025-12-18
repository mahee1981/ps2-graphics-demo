#include "renderer/renderer3d.hpp"
#include "utils.hpp"

namespace Renderer
{

Renderer3D::Renderer3D(int width, int height)
    : _screenWidth(width), _screenHeight(height),
      _perspectiveMatrix(
          ps2math::Mat4::perspective(Utils::ToRadians(60.0f), (float)width / (float)height, 0.1f, 2000.0f))
{
}

void Renderer3D::SetPerspectiveMatrix(float angleRadians, float aspectRatio, float near, float far)
{
    _perspectiveMatrix = ps2math::Mat4::perspective(angleRadians, aspectRatio, near, far);
}

void Renderer3D::ClipVertex(ps2math::Vec4 &vertex)
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
                 "sqc2       $vf7, 0x00(%0)          \n"
                 "4:                                 \n"
                 "vnop                               \n"
                 :
                 : "r"(&vertex)
                 : "$10", "memory");
}

} // namespace Renderer
