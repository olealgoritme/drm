.SUFFIXES:
.DEFAULT:

BUILDDIR ?= build
CC ?= cc
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -std=gnu99 -I/usr/include/libdrm 
LIBS = -ldrm -lEGL -lGL -lX11 -lXrender

ifeq ($(DEBUG), 1)
	CONFIG = dbg
	CFLAGS += -O0 -ggdb3
else
	CONFIG = rel
	CFLAGS += -O3
endif

DEPFLAGS = -MMD -MP
COMPILE.c = $(CC) -std=gnu99 $(CFLAGS) $(DEPFLAGS) -MT $@ -MF $@.d

OBJ := obj

SOURCES := $(wildcard */*.c)
OBJECTS := $(patsubst $(SOURCES)/%.c, $(OBJ)/%.o, $(SOURCES))

main : $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

$(OBJ)/%.o: $(SRC)/%.c
	$(CC) -I$(SRC) -c $< -o $@


