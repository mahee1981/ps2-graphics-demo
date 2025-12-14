# Project settings
SRC_DIR := src
BIN_DIR := bin
RESOURCES := resources

# ELF output
EE_BIN := $(BIN_DIR)/main.elf

# Find all sources in src/
SRCS := $(shell find $(SRC_DIR) -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.S" -o -name "*.s")
EE_OBJS := $(patsubst $(SRC_DIR)/%,$(BIN_DIR)/%,$(SRCS:.c=.o))
EE_OBJS := $(patsubst $(SRC_DIR)/%,$(BIN_DIR)/%,$(EE_OBJS:.cpp=.o))
EE_OBJS := $(patsubst $(SRC_DIR)/%,$(BIN_DIR)/%,$(EE_OBJS:.cc=.o))
EE_OBJS := $(patsubst $(SRC_DIR)/%,$(BIN_DIR)/%,$(EE_OBJS:.S=.o))
EE_OBJS := $(patsubst $(SRC_DIR)/%,$(BIN_DIR)/%,$(EE_OBJS:.s=.o))

# Libraries & includes
EE_LIBS = -ldraw -lgraph -ldma -lpacket -lpacket2 -lpadx -ldebug
EE_INCS = -Iinclude -Iexternal

# ISO target
ISO_TGT = $(BIN_DIR)/game.iso

# Default build
all: output-dirs $(EE_BIN)

# Ensure bin dir exists
output-dirs:
	@mkdir -p $(BIN_DIR)
	@mkdir -p $(BIN_DIR)/logging
	@mkdir -p $(BIN_DIR)/input
	@mkdir -p $(BIN_DIR)/graphics
	@mkdir -p $(BIN_DIR)/components
	@mkdir -p $(BIN_DIR)/resources
	@mkdir -p $(BIN_DIR)/VU0Math
	@mkdir -p $(BIN_DIR)/renderer
	@mkdir -p $(BIN_DIR)/external
	@mkdir -p $(BIN_DIR)/light
	@mkdir -p $(BIN_DIR)/CAT
	cp -r $(RESOURCES)/* $(BIN_DIR)/


# ISO build
iso: output-dirs $(EE_BIN)
	mkisofs -l -o $(ISO_TGT) $(EE_BIN) SYSTEM.CNF $(RESOURCES)

# Cleaning
clean:
	rm -rf $(EE_BIN) $(EE_OBJS) $(ISO_TGT)

# Run targets
emulate-elf: output-dirs $(EE_BIN)
	pcsx2.AppImage $(CURDIR)/$(EE_BIN)

emulate-iso: iso
	pcsx2.AppImage $(ISO_TGT)

run: $(EE_BIN)
	ps2client execee host:$(EE_BIN)

reset:
	ps2client reset

# Include PS2SDK rules + global cpp rules
include $(PS2SDK)/samples/Makefile.pref
include Makefile.eeglobal_cpp
