# TDP — Terminal Distortion Pedal
# C11, 2 threads (audio RT + UI ncurses), PortAudio + ncurses.

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -pthread
LDFLAGS ?=
LDLIBS  ?= -lportaudio -lncurses -lm

SRC_DIR := src
INC_DIR := include
BUILD   := build
TARGET  := $(BUILD)/tdp
TEST    := $(BUILD)/test_dsp

SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD)/%.o)
DEPS := $(OBJS:.o=.d)

CFLAGS += -I$(INC_DIR) -MMD -MP

.PHONY: all clean run test check check-app

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(BUILD)/%.o: $(SRC_DIR)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

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
# Le timeout 124 = arrêt propre attendu.
check-app: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=99 timeout 5 ./$(TARGET) || [ $$? -eq 124 ]

-include $(DEPS)
