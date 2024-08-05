EE_BIN = ./bin/main.elf # create one executable
EE_OBJS = ./src/GSbuffer.o src/framebuffer.o src/zbuffer.o src/DrawingEnvironment.o src/AlphaTest.o src/Color.o main.o # this is how you link individual cpp files
EE_LIBS = -ldraw -lgraph -ldma -lpacket2 -ldebug # link precompiled libs he he
EE_INCS = -I ./include

all: $(EE_BIN)
	

clean:
	rm -f $(EE_BIN) $(EE_OBJS)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

include $(PS2SDK)/samples/Makefile.pref
include ./Makefile.eeglobal_cpp