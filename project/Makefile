GRAPHICS_OBJS = bin/graphics/framebuffer.o  bin/graphics/zbuffer.o bin/graphics/GSbuffer.o  \
				bin/graphics/DrawingEnvironment.o

VU0_MATH_OBJS = bin/VU0Math/vec4.o bin/VU0Math/mat4.o 

EE_BIN = bin/main.elf 
EE_OBJS = bin/AlphaTest.o bin/Color.o bin/main.o $(VU0_MATH_OBJS) $(GRAPHICS_OBJS) bin/input/padman.o
EE_LIBS = -ldraw -lgraph -ldma -lpacket2 -ldebug -lpad 
EE_INCS = -Iinclude

all: output-dirs $(EE_BIN) 

output-dirs: 
	mkdir -p bin
	mkdir -p bin/graphics
	mkdir -p bin/input
	mkdir -p bin/VU0Math
	
clean:
	rm -rf $(EE_BIN) $(EE_OBJS)

emulate:  $(EE_BIN)
	pcsx2.AppImage $(EE_BIN)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include Makefile.eeglobal_cpp
