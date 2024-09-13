# === SETUP ===========================================================

# --- No implicit rules ---
.SUFFIXES:

# --- Paths ---
export WORKDIR = $(PWD)
export DEVKITARM = $(DEVKITPRO)/devkitARM
export COMPILED_SPRITES_DIR := $(WORKDIR)/src/data/content/_compiled_sprites
LAST_ENV_FILE := $(WORKDIR)/.last_env
LAST_BUILDTYPE_FILE := $(WORKDIR)/.last_buildtype

export LIBTONC :=              $(DEVKITPRO)/libtonc
export LIBGBA  :=              $(DEVKITPRO)/libgba
export LIBGBA_SPRITE_ENGINE := $(WORKDIR)/libs/libgba-sprite-engine
export LIBUGBA :=              $(WORKDIR)/libs/libugba

# === TONC RULES ======================================================
#
# Yes, this is almost, but not quite, completely like to
# DKP's base_rules and gba_rules
#

export PATH	:=	$(DEVKITARM)/bin:$(PATH)


# --- Executable names ---

PREFIX		?=	arm-none-eabi-

export CC	:=	$(PREFIX)gcc
export CXX	:=	$(PREFIX)g++
export AS	:=	$(PREFIX)as
export AR	:=	$(PREFIX)ar
export NM	:=	$(PREFIX)nm
export OBJCOPY	:=	$(PREFIX)objcopy

# LD defined in Makefile


# === LINK / TRANSLATE ================================================

%.gba : %.elf
	@$(OBJCOPY) -O binary $< $@
	@echo built ... $(notdir $@)
	@gbafix $@ -t$(TITLE)

#----------------------------------------------------------------------

%.mb.elf :
	@echo Linking multiboot
	$(LD) -specs=gba_mb.specs $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@
	$(NM) -Sn $@ > $(basename $(notdir $@)).map

#----------------------------------------------------------------------

%.elf :
	@echo Linking cartridge
	$(LD) -specs=gba.specs $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@
	$(NM) -Sn $@ > $(basename $(notdir $@)).map

#----------------------------------------------------------------------

%.a :
	@echo $(notdir $@)
	@rm -f $@
	$(AR) -crs $@ $^


# === OBJECTIFY =======================================================

%.iwram.o : %.iwram.cpp
	@echo $(notdir $<)
	$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) $(IARCH) -c $< -o $@

#----------------------------------------------------------------------
%.iwram.o : %.iwram.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) $(IARCH) -c $< -o $@

#----------------------------------------------------------------------

%.o : %.cpp
	@echo $(notdir $<)
	$(CXX) -MMD -MP -MF $(DEPSDIR)/$*.d $(CXXFLAGS) $(RARCH) -c $< -o $@

#----------------------------------------------------------------------

%.o : %.c
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(CFLAGS) $(RARCH) -c $< -o $@

#----------------------------------------------------------------------

%.o : %.s
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) -c $< -o $@

#----------------------------------------------------------------------

%.o : %.S
	@echo $(notdir $<)
	$(CC) -MMD -MP -MF $(DEPSDIR)/$*.d -x assembler-with-cpp $(ASFLAGS) -c $< -o $@


#----------------------------------------------------------------------
# canned command sequence for binary data
#----------------------------------------------------------------------

define bin2o
	bin2s $< | $(AS) -o $(@)
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(<F) | tr . _)`.h
	echo "extern const u8" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(<F) | tr . _)`.h
	echo "extern const u32" `(echo $(<F) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(<F) | tr . _)`.h
endef
# =====================================================================

# --- Main path ---

export PATH	:=	$(DEVKITARM)/bin:$(PATH)

# === PROJECT DETAILS =================================================
# PROJ		: Base project name
# TITLE		: Title for ROM header (12 characters)
# LIBS		: Libraries to use, formatted as list for linker flags
# BUILD		: Directory for build process temporaries. Should NOT be empty!
# SRCDIRS	: List of source file directories
# DATADIRS	: List of data file directories
# INCDIRS	: List of header file directories
# LIBDIRS	: List of library directories
# General note: use `.' for the current dir, don't leave the lists empty.

export PROJ	?= $(notdir $(CURDIR))
TITLE		:= $(PROJ)

LIBS		:= -ltonc -lgba -lgba-sprite-engine -lugba

BUILD		:= build
SRCDIRS		:= src \
						 src/data/content \
						 src/data/content/_compiled_sprites \
						 src/data/custom \
						 src/gameplay \
						 src/gameplay/debug \
						 src/gameplay/models \
						 src/gameplay/multiplayer \
						 src/gameplay/save \
						 src/gameplay/video \
						 src/objects \
						 src/objects/base \
						 src/objects/score \
						 src/objects/score/combo \
						 src/objects/ui \
						 src/player \
						 src/player/core \
						 src/scenes \
						 src/scenes/base \
						 src/scenes/ui \
						 src/utils \
						 src/utils/gbfs \
						 src/utils/flashcartio \
						 src/utils/flashcartio/everdrivegbax5 \
						 src/utils/flashcartio/ezflashomega \
						 src/utils/flashcartio/fatfs \
						 src/utils/pool \
						 libs
DATADIRS	:=
INCDIRS		:= src
LIBDIRS		:= $(LIBTONC) $(LIBGBA) $(LIBGBA_SPRITE_ENGINE) $(LIBUGBA)

# --- switches ---

bMB		:= 0	# Multiboot build
bTEMPS	:= 0	# Save gcc temporaries (.i and .S files)
bDEBUG2	:= 0	# Generate debug info (bDEBUG2? Not a full DEBUG flag. Yet)

# === BUILD FLAGS =====================================================
# This is probably where you can stop editing
# NOTE: I've noticed that -fgcse and -ftree-loop-optimize sometimes muck
#	up things (gcse seems fond of building masks inside a loop instead of
#	outside them for example). Removing them sometimes helps

# --- Architecture ---

ARCH    := -mthumb-interwork -mthumb
RARCH   := -mthumb-interwork -mthumb
IARCH   := -mthumb-interwork -marm -mlong-calls

# --- Main flags ---

CFLAGS		:= -mcpu=arm7tdmi -mtune=arm7tdmi -Ofast
CFLAGS		+= -Wall
CFLAGS		+= $(INCLUDE)
CFLAGS		+= -ffast-math -fno-strict-aliasing

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -ffunction-sections -fdata-sections -std=c++17 \
	-DLINK_CABLE_QUEUE_SIZE=10 \
	-DLINK_WIRELESS_QUEUE_SIZE=10 \
	-DLINK_WIRELESS_MAX_SERVER_TRANSFER_LENGTH=6 \
	-DLINK_WIRELESS_PUT_ISR_IN_IWRAM=1 \
	-DLINK_WIRELESS_ENABLE_NESTED_IRQ=1 \
	-DLINK_WIRELESS_USE_SEND_RECEIVE_LATCH=1 \
	-DLINK_WIRELESS_TWO_PLAYERS_ONLY=1 \
	-DLINK_DEVELOPMENT # (so gba-link-connection headers are not 'system headers' and partial builds include them)

ASFLAGS		:= $(ARCH) $(INCLUDE)
LDFLAGS 	:= $(ARCH) -Wl,--print-memory-usage,-Map,$(PROJ).map,--gc-sections

# --- switched additions ----------------------------------------------

# --- Multiboot ? ---
ifeq ($(strip $(bMB)), 1)
	TARGET	:= $(PROJ).mb
else
	TARGET	:= $(PROJ)
endif

# --- Save temporary files ? ---
ifeq ($(strip $(bTEMPS)), 1)
	CFLAGS		+= -save-temps
	CXXFLAGS	+= -save-temps
endif

# --- Debug info ? ---

ifeq ($(strip $(bDEBUG)), 1)
	CFLAGS		+= -DDEBUG -g
	CXXFLAGS	+= -DDEBUG -g
	ASFLAGS		+= -DDEBUG -g
	LDFLAGS		+= -g
else
	CFLAGS		+= -DNDEBUG
	CXXFLAGS	+= -DNDEBUG
	ASFLAGS		+= -DNDEBUG
endif

# --- Custom vars ? ---

GAMETITLE=piuGBA
GAMEMAKER=AGB
GAMECODE=V49E # Drill Dozer (Rumble - SRAM - 32kb)
MODE ?= auto
SONGS ?= src/data/content/songs
VIDEOLIB ?= src/data/content/piuGBA_videos
VIDEOENABLE ?= false
HQAUDIOLIB ?= src/data/content/piuGBA_audios
HQAUDIOENABLE ?= false
FAST ?= false
ENV ?= development
BOSS ?= true
ARCADE ?= false

ifeq ($(ENV), debug)
	CXXFLAGS += -DENV_DEBUG=true -DENV_DEVELOPMENT=true -DSENV_DEBUG=true -DSENV_DEVELOPMENT=true -DENV_ARCADE=$(ARCADE)
else ifeq ($(ENV), development)
	CXXFLAGS += -DENV_DEVELOPMENT=true -DSENV_DEVELOPMENT=true -DENV_ARCADE=$(ARCADE)
else
	CXXFLAGS += -DENV_ARCADE=$(ARCADE)
endif

# CXXFLAGS += -DCUSTOM_VAR_DEFINE

# === BUILD PROC ======================================================

ifneq ($(BUILD),$(notdir $(CURDIR)))

# Still in main dir:
# * Define/export some extra variables
# * Invoke this file again from the build dir
# PONDER: what happens if BUILD == "" ?

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export VPATH	:=									\
	$(foreach dir, $(SRCDIRS) , $(CURDIR)/$(dir))	\
	$(foreach dir, $(DATADIRS), $(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

# --- List source and data files ---

CFILES		:=	$(foreach dir, $(SRCDIRS) , $(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir, $(SRCDIRS) , $(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir, $(SRCDIRS) , $(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir, $(DATADIRS), $(notdir $(wildcard $(dir)/*.*)))

# --- Set linker depending on C++ file existence ---
ifeq ($(strip $(CPPFILES)),)
	export LD	:= $(CC)
else
	export LD	:= $(CXX)
endif

# --- Define object file list ---
export OFILES	:=	$(addsuffix .o, $(BINFILES))					\
					$(CFILES:.c=.o) $(CPPFILES:.cpp=.o)				\
					$(SFILES:.S=.o)

# --- Create include and library search paths ---
export INCLUDE	:=	$(foreach dir,$(INCDIRS),-I$(CURDIR)/$(dir))	\
					$(foreach dir,$(LIBDIRS),-I$(dir)/include)		\
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	-L$(CURDIR) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

# --- Create BUILD if necessary, and run this makefile from there ---

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	arm-none-eabi-nm -Sn $(OUTPUT).elf > $(BUILD)/$(TARGET).map

all	: $(BUILD)

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).elf $(TARGET).gba $(TARGET).sav

else		# If we're here, we should be in the BUILD dir

DEPENDS	:= $(OFILES:.o=.d)

# --- Main targets ----

$(OUTPUT).gba	:	$(OUTPUT).elf

$(OUTPUT).elf	:	$(OFILES)

$(OFILES): $(COMPILED_SPRITES_DIR)/assets.stamp

-include $(DEPENDS)

endif		# End BUILD switch

# --- More targets ----------------------------------------------------

.PHONY: check-env install check clean assets build import pkg package start rebuild restart reimport

check-env:
ifndef DEVKITPRO
	$(warning Missing environment variable: DEVKITPRO. See README.md)
	$(error "Aborting")
endif
	@if [ ! -f $(LAST_ENV_FILE) ]; then \
			echo "Last environment unknown, running clean..."; \
			echo $(ENV) > $(LAST_ENV_FILE); \
			$(MAKE) clean; \
	elif [ "$$(cat $(LAST_ENV_FILE))" != "$(ENV)" ]; then \
			echo "Environment changed from $$(cat $(LAST_ENV_FILE)) to $(ENV), running clean..."; \
			echo $(ENV) > $(LAST_ENV_FILE); \
			$(MAKE) clean; \
	fi
	@if [ ! -f $(LAST_BUILDTYPE_FILE) ]; then \
			echo "Last build type unknown, running clean..."; \
			echo $(ARCADE) > $(LAST_BUILDTYPE_FILE); \
			$(MAKE) clean; \
	elif [ "$$(cat $(LAST_BUILDTYPE_FILE))" != "$(ARCADE)" ]; then \
			echo "Build type changed from ARCADE=$$(cat $(LAST_BUILDTYPE_FILE)) to ARCADE=$(ARCADE), running clean..."; \
			echo $(ARCADE) > $(LAST_BUILDTYPE_FILE); \
			$(MAKE) clean; \
	fi

install: check-env
	./scripts/importer/install.sh

check: check-env
	./scripts/toolchain/check.sh

assets: check-env
	./scripts/assets.sh
	touch $(COMPILED_SPRITES_DIR)/assets.stamp

# build: ...

import: check-env
	./scripts/importer/run.sh --directory "$(SONGS)" --videolib="$(VIDEOLIB)" --hqaudiolib="$(HQAUDIOLIB)" --boss=$(BOSS) --arcade=$(ARCADE) --fast=$(FAST) --videoenable=$(VIDEOENABLE) --hqaudioenable=$(HQAUDIOENABLE)
	cd src/data/content/_compiled_files && gbfs ../files.gbfs *

pkg:
	./scripts/package.sh "piugba.gba" "src/data/content/files.gbfs"

package: check-env build pkg

start: check-env package
	@if command -v start > /dev/null; then \
		start "$(TARGET).out.gba"; \
	fi

rebuild: check-env clean package

restart: check-env rebuild start

reimport: check-env import package start

# EOF
