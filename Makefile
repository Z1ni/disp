ARCH?=x86_64
CC=$(ARCH)-w64-mingw32-gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Wno-unused-parameter -Iinclude/ -Ires/ -mwindows -DLOG_COLOR_OUTPUT
LIBS=-lole32 -lshlwapi -l:libconfig.a
SRCDIR=src
OBJDIR=obj
BINDIR=bin
RESDIR=res

TARGET = disp-${ARCH}

SOURCES  := $(wildcard $(SRCDIR)/*.c)
INCLUDES := $(wildcard $(SRCDIR)/*.h) $(RESDIR)/resource.h
OBJECTS  := $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# debug: CFLAGS += -O0 -g3 -DDEBUG -DGIT_VERSION=\"$(shell git describe --dirty --always --tags)\"
debug: CFLAGS += -O0 -g3 -DDEBUG
debug: $(BINDIR)/$(TARGET).exe

release: CFLAGS += -O3
release: clean $(BINDIR)/$(TARGET).exe strip

strip:
	strip -s -o $(BINDIR)/$(TARGET)-stripped.exe $(BINDIR)/$(TARGET).exe

$(OBJDIR)/resources.res:
	windres $(RESDIR)/resources.rc -O coff -o $@

# Link
$(BINDIR)/$(TARGET).exe: $(OBJECTS) $(OBJDIR)/resources.res
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

# Compile to object files
$(OBJECTS): $(OBJDIR)/%.o : $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

all: debug

.PHONY: clean all rebuild strip release debug

clean:
	rm -f obj/*.o obj/*.res bin/*.exe

rebuild: clean all
