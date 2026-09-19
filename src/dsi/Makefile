#---------------------------------------------------------------------------------
# Standalone devkitARM bring-up Makefile for the DSi port.
#
# This is deliberately NOT wired into the top-level CMakeLists.txt yet -- that
# needs a real cmake/dsi_toolchain.cmake + cmake/dsi.cmake pair mirroring
# cmake/wii.cmake/wii_toolchain.cmake, including SourceSelection.cmake picking
# the right subset of src/platform for the DSi target. That is follow-up work
# once this boots. Until then, build with:
#
#   cd src/dsi && make
#
# Needs devkitARM + libnds + libfat, e.g. from the devkitPro pacman repo:
#   sudo dkp-pacman -S nds-dev
#
# Produces DsiBringup.nds. Run it in an emulator (melonDS, no$gba) or on real
# hardware via a flashcard/TWiLight Menu++. It is the toolchain smoke test, not
# the game -- see DsiBringup.cpp for what it checks and why.
#---------------------------------------------------------------------------------
.SUFFIXES:

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

TARGET		:=	DsiBringup
BUILD		:=	build
# tools/ has the smoke-test main; system/ has the early video/memory modules it
# calls into (and DsiEarlyVideo.cpp, unused by the smoke test itself, compiles
# here too so the "real game" init code is proven against the real toolchain).
SOURCES		:=	tools system
# ".." is src/ -- both this port's own headers ("dsi/...") and the shared
# "platform/Log.h" this code logs through are rooted there.
INCLUDES	:=	..

ARCH	:=	-march=armv5te -mtune=arm946e-s -mthumb

CFLAGS	:=	-g -Wall -O2 -ffunction-sections -fdata-sections -DDSI_PLATFORM=1 $(ARCH)
CFLAGS	+=	$(INCLUDE) -DARM9
CXXFLAGS	:=	$(CFLAGS) -std=gnu++17 -fno-rtti -fno-exceptions

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:=	-lfat -lnds9

LIBDIRS	:=	$(LIBNDS)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
# platform/Log.cpp is the shared logger DsiEarlyMemory.cpp/DsiEarlyVideo.cpp call
# through (MC_LOG_INFO). Its own #if PLATFORM_WII branch is inert here (this
# build defines DSI_PLATFORM, not WII_PLATFORM), so it is safe to pull in on its
# own without the rest of src/platform, which is NOT safe: most of it assumes
# PC/PS2/Wii headers (SDL2, glad, gccore.h, ...) this Makefile does not provide.
export VPATH	+=	$(CURDIR)/../platform
export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
CPPFILES	+=	Log.cpp
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.bin)))

ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

export OFILES	:=	$(BINFILES:.bin=.o) \
					$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
					$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
					-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).nds $(TARGET).ds.gba

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).nds	: 	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

%.o	:	%.bin
	@echo $(notdir $<)
	$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
