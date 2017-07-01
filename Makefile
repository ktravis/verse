.PHONY: all clean test

BIN_DIR   = bin
SRC_DIR   = src
BUILD_DIR = $(BIN_DIR)

CFLAGS  = -Wall -std=gnu99 -g -Werror -Wno-error=unused-variable
OBJECTS = $(SRC_DIR)/verse.o $(patsubst %.c, %.o, $(shell find $(SRC_DIR)/compiler -name '*.c'))
HEADERS = $(wildcard $(SRC_DIR)/*.h)

ALL: build

build: default $(BIN_DIR)/includer $(BIN_DIR)/compiler

default:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INC) $(LIB) -c $< -o $@

$(SRC_DIR)/prelude.bin: $(SRC_DIR)/prelude.c $(BIN_DIR)/includer
	mkdir -p bin
	$(BIN_DIR)/includer $<

$(BIN_DIR)/compiler: $(OBJECTS) $(SRC_DIR)/prelude.bin
	$(CC) $(OBJECTS) $(INC) $(LIB) -o $@

$(BIN_DIR)/includer: $(SRC_DIR)/binpack.c
	mkdir -p $(BUILD_DIR)
	$(CC) $^ -o $@
	$(BIN_DIR)/includer $(SRC_DIR)/prelude.c

clean:
	-rm -f src/*.o src/**/*.o
	-rm -rf $(BUILD_DIR)
	-rm -rf $(BIN_DIR)
	-rm -f $(SRC_DIR)/prelude.bin $(SRC_DIR)/prelude.h

test: build
	@for f in tests/*.vs; do echo "Testing $$f..."; ./verse $$f >/dev/null; done;

unit-test:
	# Unit tests:
	@for f in src/*/*_test.vs src/*/*/*_test.vs; do echo "Testing $$f..."; ./verse $$f >/dev/null; done;
