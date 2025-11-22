#define STB_IMAGE_IMPLEMENTATION


#include <dma.h>
#include <draw.h>
#include <packet.h>
#include <packet2.h>
#include <stdio.h>

#include <chrono>
#include <memory>
#include <vector>

#include "VU0Math/mat4.hpp"
#include "VU0Math/vec4.hpp"
#include "graphics/DrawingEnvironment.hpp"
#include "graphics/Texture.hpp"
#include "graphics/STBITextureLoader.hpp"
#include "input/padman.hpp"
#include "renderer/Camera.hpp"
#include "utils.hpp"
#include "mesh/model.hpp"

using namespace Input;

constexpr int width = 640;
constexpr int height = 448;

constexpr float xOff = 2048.0f;
constexpr float yOff = 2048.0f;

void InitializeDMAC()
{
	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);
}


void ClipVertices(ps2math::Vec4& vertex)
{
	asm volatile(
		"lqc2       $vf7, 0x00(%0)    \n"
		"vclipw.xyz	$vf7, $vf7	    \n" // FIXME: Clip detection is still kinda broken.
		"vnop                         \n"
		"vnop                         \n"
		"vnop                         \n"
		"vnop                         \n"
		"cfc2		    $10, $18        \n"
		"beq			  $10, $0, 3f	\n"
		"2:					        \n"
		"sqc2		    $0, 0x00(%0)    \n"
		"j			    4f		    \n"
		"3:					        \n"
		"vdiv		    $Q, $vf0w, $vf7w\n"
		"vwaitq				        \n"
		"vmulq.xyz	$vf7, $vf7, $Q  \n"
		"sqc2		    $vf7, 0x00(%0)  \n"
		"4:                           \n"
		: : "r"(&vertex)
		: "$10", "memory");
}

void PrepareTriangleDisplayListForModel(packet2_t* dmaBuffer, 
										const ps2math::Mat4 &viewMatrix, 
										float angle, float moveHorizontal, 
										const Mesh& mesh,
										const Model& model)
{
	qword_t qword;
	const unsigned int numberOfTimesGifTagExecutes = (mesh.VertexIndices.size() + 1) / 3;
	// PRIM REG = 0x5B -> triangle with Gouraud shading, texturing, alpha blending
	qword.dw[0] = (u64)GIF_SET_TAG(numberOfTimesGifTagExecutes, false, true, 0x5B, GIF_FLG_PACKED, 6);

	constexpr u64 triangleGIFTag =  u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_XYZ2)<< 12 | u64(GIF_REG_RGBAQ) << 8 | u64(GIF_REG_XYZ2) << 4 | u64(GIF_REG_RGBAQ);

	// constexpr u64 triangleGIFTag = u64(GIF_REG_XYZ2) << 32 | u64(GIF_REG_RGBAQ) << 28 | u64(GIF_REG_ST) << 24 | u64(GIF_REG_XYZ2) << 20 
	// 	| u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_ST) << 12 | u64(GIF_REG_XYZ2) << 8 | u64(GIF_REG_RGBAQ) << 4 | u64(GIF_REG_ST);
	qword.dw[1] = triangleGIFTag;
	packet2_add_u128(dmaBuffer, qword.qw);

	// This line is a reminder of how incredibly silly decisions
	// can have you lose three months of development time on PS2
	// game engines
	constexpr texel_t zeroTexel = { .u = 1.0f, .v = 1.0f };

	ps2math::Vec4 scaleFactor = ps2math::Vec4(0.5f, 0.5f, 0.5f, 1.0f);
	ps2math::Mat4 perspectiveMatrix = ps2math::Mat4::perspective(Utils::ToRadians(45.0f), (float)width / (float)height, 0.1f, 2000.0f);

	for (std::size_t i = 0; i < mesh.VertexIndices.size(); i++) {

		// const auto& texel = model.GetTexturePositions()[mesh.TexIndices[i]];
		// qword.dw[0] = texel.uv;
		// qword.dw[1] = zeroTexel.uv;
		// packet2_add_u128(dmaBuffer, qword.qw);

		// color
		qword.dw[0] = (u64(128) & 0xFF) << 32 | (u64(128) & 0xFF);
		qword.dw[1] = (u64(0x80) & 0xFF) << 32 | (u64(128) & 0xFF);
		packet2_add_u128(dmaBuffer, qword.qw);

		// this copy is gonna be a performance killer, will not happen on VU1, but guarantees that it is 128-bit aligned
		// this is the one line of code that keeps my entire math from falling apart
		ps2math::Vec4 vertex = model.GetVertexPositions()[mesh.VertexIndices[i]];
		// model transformations
		ps2math::Mat4 modelMatrix;
		modelMatrix = ps2math::Mat4::scale(modelMatrix, scaleFactor);
		modelMatrix = ps2math::Mat4::rotateZ(modelMatrix, Utils::ToRadians(180.f));
		modelMatrix = ps2math::Mat4::rotateY(modelMatrix, Utils::ToRadians(angle));
		// modelMatrix = ps2math::Mat4::rotateX(modelMatrix, Utils::ToRadians(angle));
		modelMatrix = ps2math::Mat4::translate(modelMatrix, ps2math::Vec4(0.0f, 0.0f, moveHorizontal + 70.0f, 1.0f));

		// coordinates
		vertex = vertex * modelMatrix * viewMatrix * perspectiveMatrix;

		// clip vertices and perform perspective division
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

	// packet2_update(dmaBuffer, draw_finish(dmaBuffer->next));
}


void SendGIFPacketWaitForDraw(packet2_t* dmaBuffer)
{
	dma_wait_fast();

	dma_channel_send_packet2(dmaBuffer, DMA_CHANNEL_GIF, 0);

	dma_channel_wait(DMA_CHANNEL_GIF, 5000);

	packet2_reset(dmaBuffer, false);

	draw_wait_finish();

	graph_wait_vsync();
}

void SendGIFPacket(packet2_t* dmaBuffer)
{
	dma_wait_fast();

	dma_channel_send_packet2(dmaBuffer, DMA_CHANNEL_GIF, 0);

	dma_channel_wait(DMA_CHANNEL_GIF, 5000);

	packet2_reset(dmaBuffer, false);
}

Camera SetupCamera()
{
	ps2math::Vec4 startPositon{ 0.0f, 0.0f, 0.0f, 1.0f };
	ps2math::Vec4 startUp{ 0.0f, 1.0f, 0.0f, 1.0f };
	float startYaw = -90.0f;
	float startPitch = 0.0f;
	float movementSpeed = 0.05f;
	float turnSpeed =  0.05f;

	return Camera(startPositon, startUp, startYaw, startPitch, movementSpeed, turnSpeed);

}


void render()
{
	//TODO: need to make transfers dynamic and split it into slices if bigger than u16_max
	//OPTIONAL: make packed DMA transfers
	packet2_t* myDMABuffer = packet2_create(25000, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

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

	float moveHorizontal = 0.0f;

	auto textureLoader = std::make_shared<graphics::STBITextureLoader>();
	graphics::Texture myTex("CAT/TEX_CAT.PNG");
	// graphics::Texture myTex("AIRPLANE/AIRPLANE_TEX.PNG");
	myTex.LoadTexture(textureLoader);
	myTex.AllocateVram();
	myTex.TransferTextureToGS();
	myTex.SetTexSamplingMethodInGS();
	myTex.SetTextureAsActive();


	Model myModel;
	//TODO: fix the default path search
	// myModel.LoadModel("CAT/MESH_CAT.OBJ", "CAT/");
	// myModel.LoadModel("RIFLE/RIFLE.OBJ", "RIFLE/");
	// myModel.LoadModel("HITBOX/manInTheBox.obj", "HITBOX/");
	myModel.LoadModel("COMBO/combo.obj", "COMBO/");
	printf("Mesh List count: %zu\n", myModel.GetMeshList().size());
	Mesh firstMesh = myModel.GetMeshList()[0];
	printf("Number of vertices %zu\n", myModel.GetVertexPositions().size());
	printf("Number of indices in first mesh %zu\n", firstMesh.VertexIndices.size());
	printf("Number of texels %zu\n", myModel.GetTexturePositions().size());
	printf("Number of texel indices in first mesh %zu\n", firstMesh.TexIndices.size());

	PadManager controllerInput;
	auto myCamera = SetupCamera();
	while (1) {
		controllerInput.UpdatePad();

		now = std::chrono::steady_clock::now();
		deltaTime = (now - lastUpdate);
		lastUpdate = now;

		angle += (10.0f * deltaTime.count()) / 100.0f;

		if (controllerInput.getPressed().DpadRight == 1) {
			moveHorizontal += 0.01f * deltaTime.count();
		} else if (controllerInput.getPressed().DpadLeft == 1) {
			moveHorizontal += -0.01f * deltaTime.count();
		}

		if (angle > 360.0f) {
			angle = 0.0f;
		}

		myCamera.MotionControl(controllerInput.getLeftJoyPad(), deltaTime.count());
		myCamera.RotationControl(controllerInput.getRightJoyPad(),  deltaTime.count());

		drawEnv.ClearScreen(myDMABuffer);
		auto loopSize = myModel.GetMeshList().size();
		for(std::size_t i = 0; i < loopSize; i++)
		{
			const Mesh &mesh = myModel.GetMeshList()[i];
			PrepareTriangleDisplayListForModel(myDMABuffer, myCamera.CalculateViewMatrix(), angle, moveHorizontal, mesh, myModel);

			if(i == loopSize - 1)
			{
				packet2_update(myDMABuffer, draw_finish(myDMABuffer->next));
				SendGIFPacketWaitForDraw(myDMABuffer);
			}
			else{
				SendGIFPacket(myDMABuffer);
			}
		}
		// packet2_update(myDMABuffer, draw_finish(myDMABuffer->next));
		// SendGIFPacketWaitForDraw(myDMABuffer);
		//
		// // reset the buffer
		// packet2_reset(myDMABuffer, false);

	}
	packet2_free(myDMABuffer);
}

int main(int argc, char* argv[])
{
	render();

	return 0;
}
