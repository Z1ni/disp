ARCH?=x86_64
CC=$(ARCH)-w64-mingw32-gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Wno-unused-parameter -Iinclude/ -D__MINGW_USE_VC2005_COMPAT -mwindows 
LIBS=-lole32 -lconfig
SRCDIR=src
OBJDIR=obj
BINDIR=bin

TARGET = disp-${ARCH}

SOURCES  := $(filter-out $(SRCDIR)/$(EMU_TARGET).c, $(wildcard $(SRCDIR)/*.c))
INCLUDES := $(wildcard $(SRCDIR)/*.h)
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# debug: CFLAGS += -O0 -g3 -DDEBUG -DGIT_VERSION=\"$(shell git describe --dirty --always --tags)\"
debug: CFLAGS += -O0 -g3 -DDEBUG
debug: $(BINDIR)/$(TARGET).exe

release: CFLAGS += -O3
release: clean $(BINDIR)/$(TARGET).exe strip

strip:
	strip -s -o $(BINDIR)/$(TARGET)-stripped.exe $(BINDIR)/$(TARGET).exe

# Link
$(BINDIR)/$(TARGET).exe: $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

# Compile to object files
$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

all: debug

.PHONY: clean

clean:
	rm -f obj/*.o bin/*.exe
