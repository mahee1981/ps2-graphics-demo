EE_BIN = bin/main.elf 
EE_OBJS = bin/GSbuffer.o bin/framebuffer.o bin/zbuffer.o bin/DrawingEnvironment.o bin/AlphaTest.o bin/Color.o bin/VU0Math/vec4.o bin/VU0Math/mat4.o bin/main.o 
EE_LIBS = -ldraw -lgraph -ldma -lpacket2 -ldebug 
EE_INCS = -Iinclude

all: output-dirs $(EE_BIN)

output-dirs: 
	mkdir -p bin
	mkdir -p bin/VU0Math
	
clean:
	rm -f $(EE_BIN) $(EE_OBJS)

emulate: 
	pcsx2 $(EE_BIN)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include Makefile.eeglobal_cpp
