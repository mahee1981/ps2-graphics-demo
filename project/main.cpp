#include <graphics/DrawingEnvironment.hpp>
#include <graphics/framebuffer.hpp>
#include <graphics/zbuffer.hpp>

#include <input/padman.hpp>

#include <chrono>
#include <memory>
#include <vector>

#include <cmath>
#include <draw.h>
#include <dma.h>
#include <packet.h>
#include <packet2.h>
#include <stdio.h>
#include <vector>

#include <VU0Math/mat4.hpp>
#include <VU0Math/vec4.hpp>

using namespace Input;

constexpr int width = 640;
constexpr int height = 448;

constexpr float xOff = 2048.0f;
constexpr float yOff = 2048.0f;

const float pi = atan(1) * 4.0f;

float ToRadians(float angleDegrees)
{
  return angleDegrees * pi / 180.f;
}

void InitializeDMAC()
{
  dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
  dma_channel_fast_waits(DMA_CHANNEL_GIF);
}

void DumpPackets(packet2_t* packet)
{
  qword_t* qword = packet->base;
  while (qword != packet->next) {
    scr_printf("%lld %lld\n", qword->dw[0], qword->dw[1]);
    qword++;
  }
}

void ClipVertices(ps2math::Vec4& vertex)
{
  asm volatile(
      "lqc2         $vf7, 0x00(%0)  \n"
      "vclipw.xyz	$vf7, $vf7	    \n" // FIXME: Clip detection is still kinda broken.
      "vnop                         \n"
      "vnop                         \n"
      "vnop                         \n"
      "vnop                         \n"
      "cfc2		    $10, $18        \n"
      "beq			$10, $0, 3f	    \n"
      "2:					        \n"
      "sqc2		    $0, 0x00(%0)    \n"
      "j			4f		        \n"
      "3:					        \n"
      "vdiv		    $Q, $vf0w, $vf7w\n"
      "vwaitq				        \n"
      "vmulq.xyz	$vf7, $vf7, $Q  \n"
      "sqc2		    $vf7, 0x00(%0)  \n"
      "4:                           \n"
      : : "r"(&vertex)
      : "$10", "memory");
}

  float xPos = 0.0f;
void PrepareTriangleDisplayList(packet2_t* dmaBuffer, float angle, float moveHorizontal)
{

  // Data is to be stored in an obj file that has coordinates, color and texutures as Vec4, so that we get a qword alignment"
  std::vector<float> vertexData {
    10.00f, 10.00f, 10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    10.00f, 10.00f, -10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    10.00f, -10.00f, 10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    10.00f, -10.00f, -10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    -10.00f, 10.00f, 10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    -10.00f, 10.00f, -10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    -10.00f, -10.00f, 10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    -10.00f, -10.00f, -10.00f, 1.00f, 1.00f, 0.00f, 0.00f, 1.00f,
    -10.00f, 10.00f, 10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    10.00f, 10.00f, 10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    -10.00f, 10.00f, -10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    10.00f, 10.00f, -10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    -10.00f, -10.00f, 10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    10.00f, -10.00f, 10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    -10.00f, -10.00f, -10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    10.00f, -10.00f, -10.00f, 1.00f, 0.00f, 1.00f, 0.00f, 1.00f,
    -10.00f, 10.00f, 10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f,
    10.00f, 10.00f, 10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f,
    -10.00f, -10.00f, 10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f,
    10.00f, -10.00f, 10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f,
    -10.00f, 10.00f, -10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f,
    10.00f, 10.00f, -10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f,
    -10.00f, -10.00f, -10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f,
    10.00f, -10.00f, -10.00f, 1.00f, 0.00f, 0.00f, 1.00f, 1.00f

  };

  std::vector<unsigned int> indices {
    0, 1, 2, 
    1, 2, 3,
    4, 6, 5,
    5, 6, 7,
    8, 9, 10,
    9, 10, 11,
    12, 13, 14,
    13, 14, 15,
    16, 17, 18,
    17, 18, 19,
    20, 21, 22,
    21, 22, 23
  };
  //constexpr std::size_t vertexDataOffset = 0;
  constexpr std::size_t colorOffset = 4;

  constexpr std::size_t redColorOffset = colorOffset + 0;
  constexpr std::size_t blueColorOffset = colorOffset + 1;
  constexpr std::size_t greenColorOffset = colorOffset + 2;
  constexpr std::size_t alphaColorOffset = colorOffset + 3;

  constexpr std::size_t step = 8;

  qword_t qword;
  const unsigned int numberOfTimesGifTagExecutes = (indices.size() + 1) / 3;
  // 0xB = draw triangle and use Gouraud to get the color interpolation
  qword.dw[0] = (u64)GIF_SET_TAG(numberOfTimesGifTagExecutes, false, true, 0xB, GIF_FLG_PACKED, 6);
  constexpr u64 triangleGIFTag = u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_XYZ2) << 12 | u64(GIF_REG_RGBAQ) << 8 | u64(GIF_REG_XYZ2) << 4 | u64(GIF_REG_RGBAQ);

  qword.dw[1] = triangleGIFTag;
  packet2_add_u128(dmaBuffer, qword.qw);

  ps2math::Vec4 scaleFactor = ps2math::Vec4(0.5f, 0.5f, 0.5f, 1.0f);
  ps2math::Mat4 perspectiveMatrix = ps2math::Mat4::perspective(ToRadians(60.0f), (float)width / (float)height, 1.0f, 2000.0f);
  for (std::size_t i = 0; i < indices.size(); i++) {
    // color
    qword.dw[0] = (u64(vertexData[step * indices[i] + blueColorOffset] * 255.0f) & 0xFF) << 32 | (u64(vertexData[step * indices[i] + redColorOffset] * 255.0f) & 0xFF);
    qword.dw[1] = (u64(vertexData[step * indices[i] + alphaColorOffset] * 0x80) & 0xFF) << 32 | (u64(vertexData[step * indices[i] + greenColorOffset] * 255.0f) & 0xFF);
    packet2_add_u128(dmaBuffer, qword.qw);

    // this copy is gonna be a performance killer, will not happen on VU1, but guarantees that it is 128-bit aligned
    ps2math::Vec4 vertex(vertexData.data() + step * indices[i]);
    // model transformations
    ps2math::Mat4 modelMatrix;
    modelMatrix = ps2math::Mat4::scale(modelMatrix, scaleFactor);
    modelMatrix = ps2math::Mat4::rotateY(modelMatrix, ToRadians(angle));
    modelMatrix = ps2math::Mat4::rotateX(modelMatrix, ToRadians(angle));
    xPos += moveHorizontal;
    modelMatrix = ps2math::Mat4::translate(modelMatrix, ps2math::Vec4(xPos, 0.0f, 50.0f, 1.0f));

    // coordinates
    vertex = vertex * modelMatrix * perspectiveMatrix;
    
    //clip vertices and perform perspective division
    ClipVertices(vertex);

    // viewport transformation
    float winX = float(width) * vertex.x / 2.0f + (xOff);
    float winY = float(height) * vertex.y / 2.0f + (yOff);
    float deviceZ = (vertex.z + 1.0f) / 2.0f * (1 << 31);

    qword.dw[0] = (u64(Utils::FloatToFixedPoint<u16>((winY)))) << 32 | (u64(Utils::FloatToFixedPoint<u16>(winX)));
    qword.dw[1] = static_cast<unsigned int>(deviceZ);

    if ((i + 1) % 3 == 0) {
      qword.dw[1] |= (u64(1 & 0x01) << 48); // on every third vertex send a drawing kick :)
    }
    packet2_add_u128(dmaBuffer, qword.qw);
  }
  packet2_update(dmaBuffer, draw_finish(dmaBuffer->next));
}

void SendGIFPacketWaitForDraw(packet2_t* dmaBuffer)
{
  dma_wait_fast();

  dma_channel_send_normal(DMA_CHANNEL_GIF, dmaBuffer->base, packet2_get_qw_count(dmaBuffer), 0, 0);

  draw_wait_finish();

  graph_wait_vsync();
}

int main(int argc, char* argv[])
{

  packet2_t* myDMABuffer = packet2_create(50, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

  InitializeDMAC();

  printf("Starting to init\n");

  auto drawEnv = DrawingEnvironment(width, height, GraphicsConfig::SINGLE_BUFFER);

  drawEnv.InitializeEnvironment();

  printf("Done init\n");

  drawEnv.SetClearScreenColor(0, 0, 0);

  drawEnv.ClearScreen(myDMABuffer);

  packet2_update(myDMABuffer, draw_finish(myDMABuffer->next));

  SendGIFPacketWaitForDraw(myDMABuffer);

  std::chrono::duration<float, std::milli> deltaTime;
  std::chrono::steady_clock::time_point lastUpdate;
  auto now = std::chrono::steady_clock::now();
  lastUpdate = now;
  float angle = 0.0f;
  PadManager man_niko; 
  while (1) {
    man_niko.UpdatePad();
    // reset the buffer
    packet2_reset(myDMABuffer, true);

    now = std::chrono::steady_clock::now();
    deltaTime = (now - lastUpdate);
    lastUpdate = now;

    angle += (10.0f * deltaTime.count()) / 100.0f;
    float moveHorizontal = 0.0f;
    if(man_niko.getPressed().DpadRight == 1)
    {
      moveHorizontal = 0.0001 * deltaTime.count();
    }
    else if(man_niko.getPressed().DpadLeft == 1)
    {
      moveHorizontal = -0.001 * deltaTime.count();
    }


    if (angle > 360.0f) {
      angle = 0.0f;
    }

    drawEnv.ClearScreen(myDMABuffer);
    PrepareTriangleDisplayList(myDMABuffer, angle, moveHorizontal);

    SendGIFPacketWaitForDraw(myDMABuffer);
  }

  packet2_free(myDMABuffer);
  return 0;
}
