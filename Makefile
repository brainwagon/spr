CC = gcc
CFLAGS = -Wall -Wextra -O2 -I.
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

# Shaders
spr_shaders.o: spr_shaders.c spr_shaders.h spr.h stl.h
	$(CC) $(CFLAGS) -c spr_shaders.c -o spr_shaders.o

# Viewer
viewer: viewer.o stl.o spr_shaders.o libspr.a
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ viewer.o stl.o spr_shaders.o $(LDFLAGS) -lspr $(SDL_LIBS) $(LIBS)

viewer.o: viewer.c spr.h stl.h spr_shaders.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -c viewer.c -o viewer.o

# Standalone Test
test_spr: test.o libspr.a
	$(CC) $(CFLAGS) -o $@ test.o $(LDFLAGS) -lspr $(LIBS)

test.o: test.c spr.h
	$(CC) $(CFLAGS) -c test.c -o test.o

clean:
	rm -f *.o *.a viewer test_spr output_final.ppm

.PHONY: all clean
