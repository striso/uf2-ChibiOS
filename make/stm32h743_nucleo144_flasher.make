##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

# Compiler options here.
ifeq ($(USE_OPT),)
  # TODO: writing flash doesn't work with -O2, maybe caching related?
  USE_OPT = -O0 -ggdb -fomit-frame-pointer -falign-functions=16
endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT =
endif

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

# Enable this if you want the linker to remove unused code and data
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT = --print-memory-usage
endif

# Enable this if you want link time optimizations (LTO)
ifeq ($(USE_LTO),)
  USE_LTO = yes
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

# If enabled, this option makes the build process faster by not compiling
# modules not used in the current configuration.
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

# Stack size to be allocated to the Cortex-M process stack. This stack is
# the stack used by the main() thread.
ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

# Stack size to the allocated to the Cortex-M main/exceptions stack. This
# stack is used for processing interrupts and exceptions.
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

# Enables the use of FPU (no, softfp, hard).
ifeq ($(USE_FPU),)
  USE_FPU = no
endif

# FPU-related options.
ifeq ($(USE_FPU_OPT),)
  USE_FPU_OPT = -mfloat-abi=$(USE_FPU) -mfpu=fpv5-sp-d16
endif

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, sources and paths
#

# Define project name here
PROJECT = flasher
BOARD = stm32h743_nucleo144

# Target settings.
MCU  = cortex-m7

# Imported source files and paths
CHIBIOS  := ./ChibiOS
CHIBIOS_CONTRIB := $(CHIBIOS)/../ChibiOS-Contrib
CONFDIR  := ./cfg/$(BOARD)
BUILDDIR_BOOTLOADER := ./build/$(BOARD)
BUILDDIR := $(BUILDDIR_BOOTLOADER)/flasher
DEPDIR   := ./.dep/$(BOARD)_flasher
BOARDDIR := ./board/ST_NUCLEO144_H743ZI

# Licensing files.
include $(CHIBIOS)/os/license/license.mk
# Startup files.
include $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk/startup_stm32h7xx.mk
# HAL-OSAL files (optional).
include $(CHIBIOS_CONTRIB)/os/hal/hal.mk
include $(CHIBIOS_CONTRIB)/os/hal/ports/STM32/STM32H7xx/platform.mk
include $(BOARDDIR)/board.mk
include $(CHIBIOS)/os/hal/osal/rt-nil/osal.mk
# RTOS files (optional).
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC/mk/port_v7m.mk
# Other files (optional).
include $(CHIBIOS)/os/hal/lib/streams/streams.mk

# Define linker script file here
# LDSCRIPT= $(STARTUPLD)/STM32H743xI.ld
LDSCRIPT= STM32H743xI_uf2.ld

# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CSRC = $(ALLCSRC) \
       stm32h7xx_hal_flash.c \
       stm32h7xx_hal_flash_ex.c \
       bootloader.c \
       $(BUILDDIR)/bootloader_bin.c \
       flasher.c

# C++ sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CPPSRC = $(ALLCPPSRC)

# List ASM source files here.
ASMSRC = $(ALLASMSRC)

# List ASM with preprocessor source files here.
ASMXSRC = $(ALLXASMSRC)

INCDIR = $(ALLINC) $(CONFDIR) \
         $(CHIBIOS_CONTRIB)/os/various

# Define C warning options here.
CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes

# Define C++ warning options here.
CPPWARN = -Wall -Wextra -Wundef

#
# Project, target, sources and paths
##############################################################################

##############################################################################
# Start of user section
#

# List all user C define here, like -D_DEBUG=1
UDEFS =

# Define ASM defines here
UADEFS =

# List all user directories here
UINCDIR =

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS =

#
# End of user section
##############################################################################

##############################################################################
# Custom rules
#

# default: build flasher.uf2 file for updating using existing uf2 bootloader
uf2: $(BUILDDIR)/$(PROJECT).uf2
	cp $(BUILDDIR)/$(PROJECT).uf2 $(BUILDDIR_BOOTLOADER)/$(PROJECT).uf2

$(BUILDDIR)/$(PROJECT).uf2: $(BUILDDIR)/bootloader_bin.c $(BUILDDIR)/$(PROJECT).bin
	python3 uf2/utils/uf2conv.py -c -f 0xa21e1295 -b 0x08040000 $(BUILDDIR)/$(PROJECT).bin -o $(BUILDDIR)/$(PROJECT).uf2

BINARY = $(BUILDDIR_BOOTLOADER)/bootloader.bin

.PHONY: $(BINARY)
$(BINARY):
	$(MAKE) -f make/$(BOARD).make all

$(BUILDDIR)/bootloader_bin.c: $(BINARY)
	@mkdir -p $(BUILDDIR)
	python3 uf2/utils/uf2conv.py --carray $(BINARY) -o $(BUILDDIR)/bootloader_bin.c

#
# Custom rules
##############################################################################

##############################################################################
# Common rules
#

RULESPATH = $(CHIBIOS)/os/common/startup/ARMCMx/compilers/GCC/mk
include $(RULESPATH)/arm-none-eabi.mk
include $(RULESPATH)/rules.mk

#
# Common rules
##############################################################################
