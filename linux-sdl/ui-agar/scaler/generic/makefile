###
###	FM-7 EMULATOR "XM7"
###
###	Copyright (C) 2004 GIMONS
###	Copyright (C) 2010 K.Ohta
###	[ Makefile for ui-agar / scaler / sse2]
XM7_VER		= 3
include ../../../config.mak
include ../../../common.mak
DPATH = $(realpath ../../..)

COMMONSRC = \
	scaler_x05.c \
	scaler_x1.c \
	scaler_x125.c \
	scaler_x15.c \
	scaler_x2.c \
	scaler_x225.c \
	scaler_x25.c \
	scaler_x3.c \
	scaler_x4.c \
	scaler_x45.c \
	scaler_x5.c \
	scaler_x6.c


.SUFFIXES: .c .cpp .asm .o

SRCS =	$(COMMONSRC)

CSRCS =	$(filter %.c, $(SRCS))
CXXSRCS = $(filter %.cpp, $(SRCS))
CLSRCS = $(filter %.cl, $(SRCS))

OBJS = $(patsubst %.c,%.o,$(CSRCS))
OBJS += $(patsubst %.cpp,%.o,$(CXXSRCS))
OBJS += $(patsubst %.cl,%.o,$(CLSRCS))
OBJS_RELEASE = $(addprefix Release/, $(OBJS))
OBJS_DEBUG =	$(addprefix Debug/,  $(OBJS))

CFLAGS += -I../../../sdl -I../../../vm -I../../../ui-agar -I../../../fmgen 


all:	Release/libscaler-generic.a Debug/libscaler-generic.a 

Debug: Debug/libscaler-generic.a

Release: Release/libscaler-generic.a


Debug/libscaler-generic.a: ${OBJS_DEBUG}
	$(AR) crv $@ ${OBJS_DEBUG}
	ranlib $@

Release/libscaler-generic.a: ${OBJS_RELEASE}
	$(AR) crv $@ ${OBJS_RELEASE}
	ranlib $@

-include ../../../rules.mak

.PHONY: clean
clean:
	rm -f Debug/*.o Release/*.o
	rm -f Release/*.so Debug/*.so
	rm -f Debug/*.lo Release/*.lo
	rm -f Release/*.a Debug/*.a
	rm -f *~

.DEPEND:
-include $(OBJS_RELEASE:.o=.d)
-include $(OBJS_DEBUG:.o=.d)
