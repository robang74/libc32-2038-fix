
CROSS_COMPILE ?=
CC            := $(CROSS_COMPILE)gcc
STRIP         := $(CROSS_COMPILE)strip

TARGET        := libc.fix.so
SRCS          := time.c
OBJS          := $(SRCS:.c=.o)

CFLAGS        ?= -O2 -Wall -Wextra -fPIC -D_GNU_SOURCE
LDFLAGS       ?= -shared -Wl,-soname,$(TARGET)
LDLIBS        ?= -ldl

PREFIX        ?= /usr/local
LIBDIR        ?= $(PREFIX)/lib

.PHONY: all clean install strip

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(STRIP) --strip-unneeded $@

%.o: %.c time.h
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(LIBDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(LIBDIR)/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)