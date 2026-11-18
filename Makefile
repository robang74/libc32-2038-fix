
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

.PHONY: all clean install tests

all: clean $(TARGET) tests

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	$(STRIP) --strip-unneeded $@
	size $@

%.o: %.c time.h
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	install -d $(DESTDIR)$(LIBDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(LIBDIR)/$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

tests: $(TARGET)
	LD_PRELOAD=./$< date
	LD_PRELOAD=./$< date -d "2045-06-15 12:34:56"
