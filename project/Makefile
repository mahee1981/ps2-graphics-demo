# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2004, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.

EE_BIN = ./bin/main.elf # create one executable
EE_OBJS = ./src/GSbuffer.o src/framebuffer.o src/zbuffer.o src/DrawingEnvironment.o src/AlphaTest.o main.o # this is how you link individual cpp files
EE_LIBS = -ldraw -lgraph -ldma -lpacket -lpacket2 -ldebug # link precompiled libs he he
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