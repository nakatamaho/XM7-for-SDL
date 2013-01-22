###
###	FM-7 EMULATOR "XM7"
###
###	Copyright (C) 2004 GIMONS
###	Copyright (C) 2010 K.Ohta
###	[ Makefile For SDL/Agar]

#################### Build flags ############################
# Uncomment if using OpenGL
USE_OPENGL      = Yes

# Uncomment if using OpenMP
USE_OPENMP      = Yes

# Uncomment if using OpenCL for rendering
USE_OPENCL      = Yes

# Uncomment if using debugger.
WITH_DEBUGGER   = Yes

# Uncomment if BIG-ENDIAN
#BIG_ENDIAN      = Yes

# Uncomment if cross-build
#CROSS_BUILD    = Yes


# Programs
INSTALL          = install -c 
PREFIX           = /usr/local


# SET Arch specified
ARCH_FLAGS       = -mmmx -msse -msse2
#ARCH_FLAGS      = -march=amdfam10

OS               = Linux

#################### UI  ############################
## Embed目的の場合はkanameやkonatuのような小さなフォントにすること
UI_FONT            = ipagp.ttf
UI_PT              = 15

STAT_FONT          = ipagp.ttf
STAT_PT            = 9

VFD_FONT           = ipagp.ttf
CMT_FONT           = ipagp.ttf


###### Include Common.
#include common.mak




