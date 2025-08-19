GRAPHICS_OBJS = bin/graphics/framebuffer.o  bin/graphics/zbuffer.o bin/graphics/GSbuffer.o  \
				bin/graphics/DrawingEnvironment.o bin/graphics/texture.o bin/graphics/GSTextureBuffer.o \
				bin/graphics/STBITextureLoader.o bin/graphics/LodeTextureLoader.o

VU0_MATH_OBJS = bin/VU0Math/vec4.o bin/VU0Math/mat4.o 

EE_BIN = bin/main.elf 
EE_OBJS = bin/lodepng.o bin/AlphaTest.o bin/Color.o bin/main.o $(VU0_MATH_OBJS) $(GRAPHICS_OBJS) bin/input/padman.o
EE_LIBS = -ldraw -lgraph -ldma -lpacket -lpacket2 -ldebug -lpad 
EE_INCS = -Iinclude -Iexternal

ISO_TGT = bin/game.iso

RESOURCES = resources


all: output-dirs $(EE_BIN)
	cp resources/brick_wall_128.png bin/

output-dirs: 
	mkdir -p bin
	mkdir -p bin/graphics
	mkdir -p bin/input
	mkdir -p bin/VU0Math


	
iso: $(EE_BIN)
	mkisofs -l -o $(ISO_TGT) $(EE_BIN) SYSTEM.CNF $(RESOURCES) 

clean:
	rm -rf $(EE_BIN) $(EE_OBJS)

emulate-elf:  $(EE_BIN)
	pcsx2.AppImage $(EE_BIN)

emulate-iso: iso 
	pcsx2.AppImage $(ISO_TGT)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include Makefile.eeglobal_cpp
