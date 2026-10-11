# TDP — Terminal Distortion Pedal
# C11, 2 threads (audio RT + UI), PortAudio. UI : ncurses (Linux) / console (Windows).

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pthread
LDFLAGS ?=
LDLIBS  ?= -lportaudio -lncurses -lm

SRC_DIR := src
INC_DIR := include
BUILD   := build
TARGET  := $(BUILD)/tdp
TEST    := $(BUILD)/test_dsp

# Hash git court (7 chars) injecté dans le binaire.
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "nogit")
VERSION_FLAG := -DTDP_GIT_HASH=\"$(GIT_HASH)\"

# Cœur commun (réutilisé identique sur Linux et Windows).
CORE_SRCS := $(SRC_DIR)/main.c $(SRC_DIR)/audio.c $(SRC_DIR)/dsp.c $(SRC_DIR)/params.c
# UI par plateforme.
LINUX_SRCS  := $(CORE_SRCS) $(SRC_DIR)/ui_ncurses.c
WIN_SRCS    := $(CORE_SRCS) $(SRC_DIR)/ui_windows.c

OBJS := $(LINUX_SRCS:$(SRC_DIR)/%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

CFLAGS += -I$(INC_DIR) -MMD -MP $(VERSION_FLAG)

.PHONY: all clean run test check check-app windows dist

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD) dist

run: $(TARGET)
	./$(TARGET)

# Tests DSP purs : pas de PortAudio/ncurses, tourne sans carte son ni TTY.
test: $(TEST)
	./$(TEST)

$(TEST): tests/test_dsp.c $(INC_DIR)/dsp.h $(SRC_DIR)/dsp.c | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) -o $@ tests/test_dsp.c $(SRC_DIR)/dsp.c -lm

# Vérification mémoire (Valgrind). Exige `valgrind` installé.
check: test
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 ./$(TEST)

# App réelle sous Valgrind (optionnel, carte son + TTY requis).
check-app: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 timeout 5 ./$(TARGET) || [ $$? -eq 124 ]

# ---- Windows (MSYS2 / MinGW64) : tdp.exe sans ncurses ----
WIN_TARGET  := $(BUILD)/tdp.exe
WIN_OBJS    := $(WIN_SRCS:$(SRC_DIR)/%.c=$(BUILD)/win_%.o)
WIN_CFLAGS  := -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) $(VERSION_FLAG)
WIN_LDLIBS  := -lportaudio -lm

windows: $(WIN_TARGET)
$(WIN_TARGET): $(WIN_OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(WIN_CFLAGS) -o $@ $(WIN_OBJS) $(WIN_LDLIBS)

$(BUILD)/win_main.o:    $(SRC_DIR)/main.c      | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_audio.o:   $(SRC_DIR)/audio.c     | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_dsp.o:     $(SRC_DIR)/dsp.c       | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_params.o:  $(SRC_DIR)/params.c    | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_ui_windows.o: $(SRC_DIR)/ui_windows.c | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@

# Assembler dist/ (tdp.exe + portaudio.dll + README).
dist: windows
	@mkdir -p dist
	@cp $(WIN_TARGET) dist/tdp.exe
	@for d in /mingw64/bin /mingw32/bin /ucrt64/bin; do \
	  if [ -f $$d/libportaudio-0.dll ]; then cp $$d/libportaudio-0.dll dist/portaudio.dll; break; \
	  elif [ -f $$d/portaudio.dll ]; then cp $$d/portaudio.dll dist/; break; fi; \
	done
	@cp README.md dist/README.md
	@echo "dist/ prête : tdp.exe + portaudio.dll + README.md (version $(GIT_HASH))"

-include $(DEPS)
