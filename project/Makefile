GRAPHICS_OBJS = bin/graphics/framebuffer.o  bin/graphics/zbuffer.o bin/graphics/GSbuffer.o  \
				bin/graphics/DrawingEnvironment.o bin/graphics/GSTextureBuffer.o

TEXTURE_OBJS =	bin/graphics/STBITextureLoader.o bin/graphics/LodeTextureLoader.o bin/graphics/Texture.o bin/external/lodepng.o 

VU0_MATH_OBJS = bin/VU0Math/vec4.o bin/VU0Math/mat4.o 

EE_BIN = bin/main.elf 
EE_OBJS = bin/AlphaTest.o bin/Color.o bin/main.o $(VU0_MATH_OBJS) $(GRAPHICS_OBJS) $(TEXTURE_OBJS) bin/input/padman.o
EE_LIBS = -ldraw -lgraph -ldma -lpacket -lpacket2 -lpad 
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
	mkdir -p bin/external
	
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
