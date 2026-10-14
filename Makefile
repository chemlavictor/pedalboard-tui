# TPD — Terminal Pedal Board (CLI)
# C11, 2 threads (audio RT + main CLI), PortAudio. UI : shell de commandes (portable).

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pthread
LDFLAGS ?=
LDLIBS  ?= -lportaudio -lm

SRC_DIR := src
INC_DIR := include
BUILD   := build
TARGET  := $(BUILD)/tdp
TEST    := $(BUILD)/test_dsp
TEST_TPD := $(BUILD)/test_tdp
TEST_WAV := $(BUILD)/test_wav
TEST_TUNER := $(BUILD)/test_tuner

# Hash git court (7 chars) + date du build (YYYY.MM.DD) injectés dans le binaire.
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "nogit")
BUILD_DATE := $(shell date +%Y.%m.%d 2>/dev/null || echo "1970.01.01")
VERSION_FLAG := -DTPD_GIT_HASH=\"$(GIT_HASH)\" -DTPD_BUILD_DATE=\"$(BUILD_DATE)\"

# Cœur commun (CLI) — réutilisé identique sur Linux et Windows.
CORE_SRCS := $(SRC_DIR)/main.c $(SRC_DIR)/shell.c $(SRC_DIR)/pedals.c \
             $(SRC_DIR)/audio.c $(SRC_DIR)/dsp.c $(SRC_DIR)/params.c $(SRC_DIR)/wav.c $(SRC_DIR)/tuner.c $(SRC_DIR)/devtest.c $(SRC_DIR)/in_test.c $(SRC_DIR)/server.c

LINUX_SRCS  := $(CORE_SRCS)
WIN_SRCS    := $(CORE_SRCS)

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

# Tests DSP + catalogue : pas de PortAudio, tourne sans carte son ni TTY.
test: $(TEST) $(TEST_TPD) $(TEST_WAV) $(TEST_TUNER)
	./$(TEST)
	./$(TEST_TPD)
	./$(TEST_WAV)
	./$(TEST_TUNER)

$(TEST): tests/test_dsp.c $(INC_DIR)/dsp.h $(INC_DIR)/pedals.h $(SRC_DIR)/dsp.c $(SRC_DIR)/pedals.c | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) -o $@ tests/test_dsp.c $(SRC_DIR)/dsp.c $(SRC_DIR)/pedals.c -lm

$(TEST_TPD): tests/test_tdp.c $(INC_DIR)/shell.h $(INC_DIR)/pedals.h $(SRC_DIR)/shell.c $(SRC_DIR)/pedals.c $(SRC_DIR)/wav.c $(SRC_DIR)/audio.c $(SRC_DIR)/tuner.c $(SRC_DIR)/devtest.c $(SRC_DIR)/in_test.c | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) -o $@ tests/test_tdp.c $(SRC_DIR)/shell.c $(SRC_DIR)/pedals.c $(SRC_DIR)/wav.c $(SRC_DIR)/audio.c $(SRC_DIR)/tuner.c $(SRC_DIR)/devtest.c $(SRC_DIR)/in_test.c -lportaudio -lm

$(TEST_WAV): tests/test_wav.c $(INC_DIR)/wav.h $(SRC_DIR)/wav.c | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) -o $@ tests/test_wav.c $(SRC_DIR)/wav.c -lm

$(TEST_TUNER): tests/test_tuner.c $(INC_DIR)/tuner.h $(SRC_DIR)/tuner.c | $(BUILD)
	$(CC) -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) -o $@ tests/test_tuner.c $(SRC_DIR)/tuner.c -lm

# Vérification mémoire (Valgrind). Exige `valgrind` installé.
check: test
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 ./$(TEST)

# App réelle sous Valgrind (optionnel, carte son + TTY requis).
check-app: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 timeout 5 ./$(TARGET) || [ $$? -eq 124 ]

# ---- Windows (MSYS2 / MinGW64) : tdp.exe (CLI, sans ncurses) ----
WIN_TARGET  := $(BUILD)/tdp.exe
WIN_OBJS    := $(WIN_SRCS:$(SRC_DIR)/%.c=$(BUILD)/win_%.o)
WIN_CFLAGS  := -std=c11 -O2 -Wall -Wextra -I$(INC_DIR) $(VERSION_FLAG)
WIN_LDLIBS  := -lportaudio -lws2_32 -lm

windows: $(WIN_TARGET)
$(WIN_TARGET): $(WIN_OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(WIN_CFLAGS) -o $@ $(WIN_OBJS) $(WIN_LDLIBS)

$(BUILD)/win_main.o:    $(SRC_DIR)/main.c    | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_shell.o:   $(SRC_DIR)/shell.c   | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_pedals.o:  $(SRC_DIR)/pedals.c  | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_audio.o:   $(SRC_DIR)/audio.c   | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_dsp.o:     $(SRC_DIR)/dsp.c     | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_params.o:  $(SRC_DIR)/params.c  | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_wav.o:     $(SRC_DIR)/wav.c     | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_tuner.o:   $(SRC_DIR)/tuner.c   | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_devtest.o: $(SRC_DIR)/devtest.c | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_in_test.o: $(SRC_DIR)/in_test.c | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@
$(BUILD)/win_server.o: $(SRC_DIR)/server.c | $(BUILD)
	$(CC) $(WIN_CFLAGS) -c $< -o $@

# Assembler dist/ (tdp.exe + portaudio.dll + README).
dist: windows
	@mkdir -p dist
	@cp $(WIN_TARGET) dist/tdp.exe
	@copied=0; \
	for f in /mingw64/bin/libportaudio.dll /mingw32/bin/libportaudio.dll /ucrt64/bin/libportaudio.dll \
	         /mingw64/bin/libportaudio-0.dll /mingw32/bin/libportaudio-0.dll /ucrt64/bin/libportaudio-0.dll \
	         /mingw64/bin/portaudio.dll /mingw32/bin/portaudio.dll /ucrt64/bin/portaudio.dll; do \
	  if [ -f "$$f" ]; then cp "$$f" dist/portaudio.dll; echo "  DLL: $$f -> dist/portaudio.dll"; copied=1; break; fi; \
	done; \
	if [ "$$copied" = "0" ]; then \
	  echo "  DLL introuvable localement -> telechargement..."; \
	  bash download_portaudio_dll.sh || echo "  ECHec telechargement. Copie manuellement portaudio.dll dans dist/"; \
	fi
	@cp README.md dist/README.md
	@echo "dist/ prete (version $(GIT_HASH)) :"; \
	ls -la dist/

-include $(DEPS)
