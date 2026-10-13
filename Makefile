# Explorer 3 for the Raspberry Pi. On the Pi, or on a Debian PC (also to
# build for the Pi: CROSS=aarch64-linux-gnu- or CROSS=arm-linux-gnueabihf-,
# with libsdl2-dev of that architecture). Needs tools/fetch-deps.sh first.
#
#   make                 build/explorer3
#   make deb             dist/explorer3_<version>_<arch>.deb (tools/make-deb.sh)
#   make clean

GAME_DIR ?= deps/Escape-Game-Cardputer
GFX_DIR  ?= deps/M5GFX/src
BUILD    ?= build
CROSS    ?=
CC       := $(CROSS)gcc
CXX      := $(CROSS)g++
PKG_CONFIG ?= pkg-config
VERSION  ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo dev)

GEN := $(BUILD)/gen
SDL_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2)
SDL_LIBS   := $(shell $(PKG_CONFIG) --libs sdl2)

OPT      := -O2 -ffunction-sections -fdata-sections
CXXSTD   := -std=gnu++17
# The game and M5GFX: M5GFX's Linux back end on a memory panel (display_m5gfx.cpp)
GFX_DEFS := -DLGFX_LINUX_FB -DM5GFX_BOARD=board_M5Cardputer
GAME_INC := -Isrc/include -Isrc -I$(GAME_DIR)/sim/include -I$(GEN) -I$(GFX_DIR)
CTRL_INC := -Isrc/control -I$(GEN)/control

# Pi code: warnings on; the game and M5GFX as they are
PI_CXXFLAGS   := $(CXXSTD) $(OPT) -Wall $(GFX_DEFS) $(GAME_INC) $(CTRL_INC) $(SDL_CFLAGS)
GAME_CXXFLAGS := $(CXXSTD) $(OPT) -w $(GFX_DEFS) $(GAME_INC)
CTRL_CFLAGS   := -std=gnu11 $(OPT) -Wall $(CTRL_INC) $(SDL_CFLAGS)
LDFLAGS  += -Wl,--gc-sections
LDLIBS   := $(SDL_LIBS) -lpthread -lm

PI_SRC   := audio.cpp display_m5gfx.cpp game.cpp input.cpp labels.cpp main.cpp mirror_pi.cpp netctl.cpp \
            onscreen_keyboard.cpp role_menu.cpp
CTRL_SRC := addr.c banner.c clocksync.c control.c draw.c font_8x16.c msg.c net.c panel.c rows.c sdl_draw.c \
            session.c sound.c text.c texts.c ui.c ws.c
GFX_SRC  := $(wildcard $(GFX_DIR)/lgfx/v1/*.cpp) $(GFX_DIR)/lgfx/v1/panel/Panel_M5HDMI.cpp
GFX_CSRC := $(wildcard $(GFX_DIR)/lgfx/utility/*.c) $(wildcard $(GFX_DIR)/lgfx/Fonts/efont/*.c) \
            $(wildcard $(GFX_DIR)/lgfx/Fonts/IPA/*.c)

OBJ := $(PI_SRC:%.cpp=$(BUILD)/pi/%.o) \
       $(BUILD)/game/main.o \
       $(CTRL_SRC:%.c=$(BUILD)/control/%.o) \
       $(patsubst $(GFX_DIR)/%.cpp,$(BUILD)/gfx/%.o,$(GFX_SRC)) \
       $(patsubst $(GFX_DIR)/%.c,$(BUILD)/gfx/%.o,$(GFX_CSRC))

all: $(BUILD)/explorer3

# The game's sources, changed for the Pi (texts, tr(), bridge.inc)
$(GEN)/.stamp: tools/prepare.sh tools/*.sed src/bridge.inc $(GAME_DIR)/src/main.cpp $(GAME_DIR)/src/textes.h \
               $(GAME_DIR)/src/diffusion.cpp $(GAME_DIR)/src/diffusion.h $(GAME_DIR)/src/mots.h \
               $(GAME_DIR)/src/dessins.h tools/page-texts.awk
	GAME_DIR=$(GAME_DIR) OUT=$(GEN) sh tools/prepare.sh
	touch $@

$(BUILD)/pi/%.o: src/%.cpp $(GEN)/.stamp
	@mkdir -p $(dir $@)
	$(CXX) $(PI_CXXFLAGS) -DEXPLORER3_VERSION='"$(VERSION)"' -MMD -c $< -o $@

$(BUILD)/game/main.o: $(GEN)/.stamp
	@mkdir -p $(dir $@)
	$(CXX) $(GAME_CXXFLAGS) -MMD -c $(GEN)/main.cpp -o $@

$(BUILD)/control/%.o: src/control/%.c $(GEN)/.stamp
	@mkdir -p $(dir $@)
	$(CC) $(CTRL_CFLAGS) -MMD -c $< -o $@

$(BUILD)/gfx/%.o: $(GFX_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(GAME_CXXFLAGS) -c $< -o $@

$(BUILD)/gfx/%.o: $(GFX_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -std=gnu11 $(OPT) -w $(GFX_DEFS) -I$(GFX_DIR) -c $< -o $@

$(BUILD)/explorer3: $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

deb: $(BUILD)/explorer3
	VERSION=$(VERSION) BUILD=$(BUILD) CROSS=$(CROSS) sh tools/make-deb.sh

clean:
	rm -rf $(BUILD) dist

.PHONY: all deb clean

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
