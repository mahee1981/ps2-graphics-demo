#include <framebuffer.hpp>
#include <zbuffer.hpp>
#include <DrawingEnvironment.hpp>

#include <dma.h>

#include <memory>
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

void InitalizeGS(std::shared_ptr<Buffers::Framebuffer> &framebuffer, std::shared_ptr<Buffers::ZBuffer> &zbuffer)
{
    framebuffer = std::make_shared<Buffers::Framebuffer>(width, height, 0, Buffers::GSPixelStorageMethod::PSM_32);
    zbuffer = std::make_shared<Buffers::ZBuffer>(width, height, 0, true, Buffers::ZbufferTestMethod::GREATER_EQUAL, Buffers::GSZbufferStorageMethodEnum::ZBUF_32);

    graph_set_mode(GRAPH_MODE_INTERLACED, GRAPH_MODE_NTSC, GRAPH_MODE_FIELD, GRAPH_ENABLE);
    graph_set_screen(0, 0, width, height); // TODO: learn more about this in docs
    graph_set_bgcolor(0, 0, 0);
    framebuffer->SetFramebufferAsActiveFilteredMode();
    graph_enable_output();

}



int main(int argc, char *argv[])
{


    std::shared_ptr<Buffers::Framebuffer> framebuffer(nullptr);
    std::shared_ptr<Buffers::ZBuffer> zbuffer(nullptr);
    std::shared_ptr<AlphaTest> alphaTest(std::make_shared<AlphaTest>(true, Buffers::AlphaTestMethod::NOT_EQUAL, 0x00, Buffers::AlphaTestOnFail::FB_UPDATE_ONLY));

    InitializeDMAC();

    printf("Starting to init\n");
    InitalizeGS(framebuffer, zbuffer);
    DrawingEnvironment draw_env(framebuffer, zbuffer, alphaTest);
    draw_env.SetupDrawingEnvironment(0);
    printf("Done init\n");
    
    packet_t *packet = packet_init(400,PACKET_NORMAL);

    // Used for the qword pointer.
    qword_t *q;

    // Since we only have one packet, we need to wait until the dmac is done
    // before reusing our pointer;
    dma_wait_fast();

    q = packet->data;
    q = draw_clear(q,0,(2048-320),(2048-224),framebuffer->GetWidth(),framebuffer->GetHeight(),100,0,0); // hate this function,
    q = draw_finish(q);

    dma_channel_send_normal(DMA_CHANNEL_GIF,packet->data, q - packet->data, 0, 0);

    // Wait until the screen is cleared.
    draw_wait_finish();

    // Update the screen.
    graph_wait_vsync();
    
    scr_printf("Successfully initialized the PS2 Graphics Synthesizer!");

    while(1){

    }


    return 0;

   
}