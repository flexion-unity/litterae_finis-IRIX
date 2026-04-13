CXX = g++
CC  = gcc

TARGET = litterae-finis

# OS detection
OS := $(shell uname -s)

ifeq ($(OS),IRIX64)
  IRIX := 1
else ifeq ($(OS),IRIX)
  IRIX := 1
endif


# Base linker flags (platform appends below)
LDFLAGS :=

# Platform-specific flags

ifdef IRIX
  # MIPS n32 ABI standard for GCC on IRIX 6.5
  ABI_FLAGS   := -mabi=n32
  SDL2_CFLAGS ?= -I/usr/local/include/SDL2 -D_REENTRANT -D_SGI_MP_SOURCE -Wl,--allow-shlib-undefined
  ifeq ($(STATIC),1)
    # Static SDL2: make STATIC=1
    # SGUG SDL2 was built with SDL_VIDEO_DRIVER_X11_DYNAMIC, so it dlopen's
    # X11 at runtime — do not link X11 explicitly, which keeps all SGUG X11
    # sonames out of DT_NEEDED.
    # --unresolved-symbols=ignore-in-object-files suppresses the unresolved
    # X11/GL references from SDL2.a at link time (SDL2 resolves them itself).
    SDL2_LIBS ?= /usr/sgug/lib32/libSDL2_mixer.a \
                 /usr/sgug/lib32/libSDL2.a \
                 -laudio -lpthread -lm \
                 -Wl,--unresolved-symbols=ignore-in-object-files
    LDFLAGS   += -static-libstdc++ -static-libgcc

  else
    # Dynamic (default). Override SDL2_LIBS if SDL2 is not in /usr/sgug.
    #   make SDL2_LIBS="-L/usr/freeware/lib32 -lSDL2 -lSDL2_mixer"
    SDL2_LIBS ?= -L/usr/sgug/lib32 -lSDL2 -lSDL2_mixer
  endif
else
  	# Linux / other POSIX use pkg-config
  	ABI_FLAGS :=
    SDL2_CFLAGS ?= $(shell pkg-config --cflags sdl2 SDL2_mixer 2>/dev/null)
	SDL2_LIBS   ?= $(shell pkg-config --libs   sdl2 SDL2_mixer 2>/dev/null)
endif


# Compiler flags

CXXFLAGS = -O2 -Wall -Wno-unused-result \
           -I./include -I./stb -I./tfx -I./glm \
           $(ABI_FLAGS) $(SDL2_CFLAGS)

CFLAGS   = -O2 \
           -I./include -I./stb -I./glm \
           $(ABI_FLAGS)

LDFLAGS  += $(ABI_FLAGS)


# Sources

SRCS_CXX = \
	demo.cpp \
	zoomer.cpp \
	greets.cpp \
	intro.cpp \
	vertexbuffer.cpp \
	sphere.cpp \
	tfx/tfx_unixmain.cpp \
	tfx/tfx_asciiart.cpp \
	tfx/tfx_blockcolor.cpp \
	tfx/tfx_bruteforce.cpp \
	tfx/tfx_col_asciiart.cpp \
	tfx/tfx_dither.cpp \
	tfx/tfx_fontdata.cpp \
	tfx/tfx_halfblockcolor.cpp \
	tfx/tfx_tcconverter.cpp \
	zgl/api.cpp \
	zgl/arrays.cpp \
	zgl/clear.cpp \
	zgl/clip.cpp \
	zgl/error.cpp \
	zgl/get.cpp \
	zgl/image_util.cpp \
	zgl/init.cpp \
	zgl/light.cpp \
	zgl/list.cpp \
	zgl/matrix.cpp \
	zgl/memory.cpp \
	zgl/misc.cpp \
	zgl/msghandling.cpp \
	zgl/oscontext.cpp \
	zgl/select.cpp \
	zgl/specbuf.cpp \
	zgl/texture.cpp \
	zgl/tfxswgl.cpp \
	zgl/vertex.cpp \
	zgl/zbuffer.cpp \
	zgl/zline.cpp \
	zgl/ztriangle.cpp

SRCS_C = \
	stb/stb_image.c \
	zgl/glu/glu.c \
	zgl/glu/glu_perspective.c

OBJS = $(SRCS_CXX:.cpp=.o) $(SRCS_C:.c=.o)

# Build rules

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(SDL2_LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
