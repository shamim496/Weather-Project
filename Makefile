# ---------------------------------------------------------------
#  Weather Forecasting System - Makefile
#  make          build the program  (bin/weather)
#  make run      build and run
#  make test     build and run the unit tests
#  make clean    remove build files
# ---------------------------------------------------------------
CC      = gcc
CFLAGS  ?= -std=c99 -Wall -Wextra -pedantic -O2
CFLAGS  += -Iinclude
LDLIBS  += -lm

SRC_DIR  := src
OBJ_DIR  := build
BIN_DIR  := bin

ifeq ($(OS),Windows_NT)
    EXE := .exe
else
    EXE :=
endif

TARGET      := $(BIN_DIR)/weather$(EXE)
TEST_TARGET := $(BIN_DIR)/run_tests$(EXE)

SOURCES     := $(wildcard $(SRC_DIR)/*.c)
OBJECTS     := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SOURCES))
# every module except main.c and the interactive menus is unit tested
LIB_OBJECTS := $(filter-out $(OBJ_DIR)/main.o $(OBJ_DIR)/user_menu.o $(OBJ_DIR)/admin_menu.o,$(OBJECTS))

.PHONY: all run test clean

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(wildcard include/*.h) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_TARGET): tests/test_main.c $(LIB_OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
