#include <framebuffer.hpp>
#include <zbuffer.hpp>
#include <DrawingEnvironment.hpp>

#include <dma.h>

#include <memory>
#include <vector>
#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <packet2.h>
#include <packet.h>
#include <unistd.h>
#include <debug.h>
#include <draw.h>
#include <draw2d.h>


constexpr int width = 640;
constexpr int height = 448;

constexpr float xOff = 2048.0f;
constexpr float yOff = 2048.0f;

void InitializeDMAC()
{
	dma_channel_initialize(DMA_CHANNEL_GIF,NULL,0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);
}

void DumpPackets(packet2_t *packet)
{
    qword_t *qword = packet->base;
    while(qword != packet->next) {
        scr_printf("%lld %lld\n", qword->dw[0], qword->dw[1]);
        qword++;
    }
}

void InitalizeGS(std::shared_ptr<Buffers::Framebuffer> &framebuffer, std::shared_ptr<Buffers::ZBuffer> &zbuffer, std::shared_ptr<AlphaTest> &alphaTest)
{
    framebuffer = std::make_shared<Buffers::Framebuffer>(width, height, 0, Buffers::GSPixelStorageMethod::PSM_32);
    zbuffer = std::make_shared<Buffers::ZBuffer>(width, height, 0, true, Buffers::ZbufferTestMethod::GREATER_EQUAL, Buffers::GSZbufferStorageMethodEnum::ZBUF_32);
    alphaTest = std::make_shared<AlphaTest>(true, Buffers::AlphaTestMethod::NOT_EQUAL, 0x00, Buffers::AlphaTestOnFail::FB_UPDATE_ONLY);

    graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
    graph_set_screen(0, 0, width, height); // TODO: learn more about this in docs
    graph_set_bgcolor(0, 0, 0);
    framebuffer->SetFramebufferAsActiveFilteredMode();
    graph_enable_output();

}

packet2_t *RenderTriangle()
{
    std::vector<float> triangleData
    {
        //x     y     z     r     g     b
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 100.f, 0.0f, 0.0f, 1.0f, 0.0f,
        100.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f

    };

    packet2_t *packet = packet2_create(20, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);

    qword_t qword;
    // 0xB = draw triangle and use Gouraud to get the color interpolation
    qword.dw[0] = (u64)GIF_SET_TAG(1, false, true, 0xB, GIF_FLG_PACKED, 6);
    qword.dw[1] = u64(GIF_REG_XYZ2) << 20 | u64(GIF_REG_RGBAQ) << 16 | u64(GIF_REG_XYZ2) << 12 | u64(GIF_REG_RGBAQ) << 8 | u64(GIF_REG_XYZ2) << 4 | u64(GIF_REG_RGBAQ);
    packet2_add_u128(packet, qword.qw);

    for(std::size_t i = 0; i < triangleData.size(); i+=6) 
    {
        //color
        qword.dw[0] = (u64(triangleData[i+4]*255.0f) & 0xFF) << 32 | (u64(triangleData[i+3]*255.0f) & 0xFF);
        qword.dw[1] = (u64(0x40 & 0xFF) << 32 | (u64(triangleData[i+5]*255.0f) & 0xFF));
        packet2_add_u128(packet, qword.qw);


        //coordinates
        qword.dw[0] = (u64(Utils::FloatToFixedPoint(triangleData[i+1] + yOff))) << 32  | (u64(Utils::FloatToFixedPoint(triangleData[i] + xOff) ));
        qword.dw[1] = u64(triangleData[i+2]) & 0xFFFFFFFF;

        if(i == triangleData.size() - 6)
        {
            qword.dw[1] |= (u64(1 & 0x01) << 48);
        }
        packet2_add_u128(packet, qword.qw);

    }
    packet2_update(packet, draw_finish(packet->next));

    
    return packet;
}



int main(int argc, char *argv[])
{
    std::shared_ptr<Buffers::Framebuffer> framebuffer(nullptr);
    std::shared_ptr<Buffers::ZBuffer> zbuffer(nullptr);
    std::shared_ptr<AlphaTest> alphaTest(nullptr);

    InitializeDMAC();

    printf("Starting to init\n");
    InitalizeGS(framebuffer, zbuffer, alphaTest);

    DrawingEnvironment draw_env(framebuffer, zbuffer, alphaTest);

    draw_env.SetupDrawingEnvironment(0);

    printf("Done init\n");
    
    packet_t *packet = packet_init(50,PACKET_NORMAL);


    qword_t *q;

    // Since we only have one packet, we need to wait until the dmac is done
    // before reusing our pointer;
    dma_wait_fast();

    //to be replaced with my own clear screen!
    q = packet->data;
    q = draw_clear(q,0,(2048-320),(2048-224),framebuffer->GetWidth(),framebuffer->GetHeight(),0,0,0); // hate this function, check if it overwrites PMODE register
    q = draw_finish(q);

    dma_channel_send_normal(DMA_CHANNEL_GIF,packet->data, q - packet->data, 0, 0);

    // Wait until the screen is cleared.
    draw_wait_finish();

    // Update the screen.
    graph_wait_vsync();
    
    scr_printf("Successfully initialized the PS2 Graphics Synthesizer!\n");
    scr_printf("Also rendered a triangle!\n");

    auto my_packet = RenderTriangle();

    packet2_print(my_packet, packet2_get_qw_count(my_packet));


    while(1){

        dma_wait_fast();

        dma_channel_send_normal(DMA_CHANNEL_GIF, my_packet->base, packet2_get_qw_count(my_packet), 0, 0);

        draw_wait_finish();
        
        graph_wait_vsync();
    }

    packet2_free(my_packet);
    packet_free(packet);
    return 0;

   
}