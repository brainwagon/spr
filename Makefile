CC = gcc
CFLAGS = -Wall -Wextra -O2 -Isrc -DSPR_ENABLE_TEXTURES
LDFLAGS = -Llib -lspr -lm
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS := $(shell sdl2-config --libs)

SRCDIR = src
LIBDIR = lib
BINDIR = bin

LIB_SRCS = $(wildcard $(SRCDIR)/*.c)
LIB_OBJS = $(LIB_SRCS:.c=.o)

all: dirs lib viewer template test

dirs:
	mkdir -p $(LIBDIR) $(BINDIR)

lib: $(LIB_OBJS)
	ar rcs $(LIBDIR)/libspr.a $(LIB_OBJS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

viewer: apps/viewer/main.c lib
	$(CC) $(CFLAGS) $(SDL_CFLAGS) apps/viewer/main.c -o $(BINDIR)/viewer $(LDFLAGS) $(SDL_LIBS)

template: apps/template/main.c lib
	$(CC) $(CFLAGS) $(SDL_CFLAGS) apps/template/main.c -o $(BINDIR)/template $(LDFLAGS) $(SDL_LIBS)

test: apps/test_headless/main.c lib
	$(CC) $(CFLAGS) apps/test_headless/main.c -o $(BINDIR)/test $(LDFLAGS)

clean:
	rm -f $(SRCDIR)/*.o $(LIBDIR)/*.a $(BINDIR)/*

.PHONY: all clean dirs lib viewer template test