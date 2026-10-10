CC ?= cc

CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Iinclude

CPPFLAGS += $(shell pkg-config --cflags libcurl openssl)
LDLIBS += $(shell pkg-config --libs libcurl openssl)

PREFIX ?= $(HOME)/.local
DESTDIR ?=

SOURCES := src/main.c src/util.c src/manifest.c src/repo.c src/package.c src/install.c
OBJECTS := $(SOURCES:.c=.o)

.PHONY: all clean install check

all: build/forge

build/forge: $(OBJECTS)
	@mkdir -p build
	$(CC) $(CFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

src/%.o: src/%.c include/forge.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: build/forge
	install -Dm755 build/forge "$(DESTDIR)$(PREFIX)/bin/forge"

clean:
	rm -f $(OBJECTS) build/forge build/test_manifest
