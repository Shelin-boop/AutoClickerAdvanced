# AutoClicker Advanced - build
# Needs: gcc, pkg-config, glib-compile-resources and the development packages
# for GTK 3, X11, Xi, Xext and cairo.
PKGS    = gtk+-3.0 x11 xi xext cairo cairo-xlib
CFLAGS ?= -O2
CFLAGS += -std=gnu11 -Wall -Wextra -Wno-unused-parameter $(shell pkg-config --cflags $(PKGS))
LIBS    = $(shell pkg-config --libs $(PKGS)) -lm
SRC     = src/main.c src/config.c src/x11api.c src/overlay.c src/hotkeys.c \
          src/sequencer.c src/integration.c src/mainwin.c src/settings.c \
          src/match.c src/capture.c src/react.c src/editor.c
BUILD   = build
BIN     = $(BUILD)/autoclicker-advanced

all: $(BIN)

$(BUILD)/resources.c: src/resources/xca.gresource.xml src/resources/icon.png
	@mkdir -p $(BUILD)
	glib-compile-resources --sourcedir=src/resources --generate-source --target=$@ $<

$(BIN): $(SRC) src/*.h $(BUILD)/resources.c
	$(CC) $(CFLAGS) -Isrc -o $@ $(SRC) $(BUILD)/resources.c $(LIBS)

appimage: $(BIN)
	./pkg/build-appimage.sh

clean:
	rm -rf $(BUILD)

.PHONY: all appimage clean
