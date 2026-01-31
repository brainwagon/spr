CC = gcc
CFLAGS = -Wall -Wextra -O2 -I. -DSPR_ENABLE_TEXTURES
LDFLAGS = -L.
LIBS = -lm
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

# Targets
all: viewer

# Library
libspr.a: spr.o
	ar rcs $@ $^

spr.o: spr.c spr.h
	$(CC) $(CFLAGS) -c spr.c -o spr.o

# STL Loader
stl.o: stl.c stl.h
	$(CC) $(CFLAGS) -c stl.c -o stl.o

# Textures
spr_texture.o: spr_texture.c spr_texture.h spr.h stb_image.h
	$(CC) $(CFLAGS) -c spr_texture.c -o spr_texture.o

# Loader
spr_loader.o: spr_loader.c spr_loader.h stl.h spr.h spr_texture.h
	$(CC) $(CFLAGS) -c spr_loader.c -o spr_loader.o

# Shaders
spr_shaders.o: spr_shaders.c spr_shaders.h spr.h stl.h spr_texture.h
	$(CC) $(CFLAGS) -c spr_shaders.c -o spr_shaders.o

# Viewer
viewer: viewer.o stl.o spr_shaders.o spr_texture.o spr_loader.o libspr.a
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ viewer.o stl.o spr_shaders.o spr_texture.o spr_loader.o $(LDFLAGS) -lspr $(SDL_LIBS) $(LIBS)

viewer.o: viewer.c spr.h stl.h spr_shaders.h spr_loader.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c viewer.c -o viewer.o

# Standalone Test
test_spr: test.o libspr.a
	$(CC) $(CFLAGS) -o $@ test.o $(LDFLAGS) -lspr $(LIBS)

test.o: test.c spr.h
	$(CC) $(CFLAGS) -c test.c -o test.o

clean:
	rm -f *.o *.a viewer test_spr output_final.ppm

.PHONY: all clean
