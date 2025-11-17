CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -pedantic -I./include
LDFLAGS = -lm
DEBUGFLAGS = -g -O0 -coverage
RELEASEFLAGS = -O3 -march=native

SRC_DIR = src
INC_DIR = include
TEST_DIR = tests
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))

# Test files
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_BINS = $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(TEST_SRCS))

# Main target
TARGET = $(BIN_DIR)/ascii_cube

.PHONY: all clean test install debug release

all: debug

# Debug build
debug: CFLAGS += $(DEBUGFLAGS)
debug: $(TARGET)

# Release build
release: CFLAGS += $(RELEASEFLAGS)
release: $(TARGET)

# Create directories
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link main executable
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

# Build tests
$(BIN_DIR)/test_%: $(TEST_DIR)/test_%.c $(filter-out $(OBJ_DIR)/main.o,$(OBJS)) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(DEBUGFLAGS) $< $(filter-out $(OBJ_DIR)/main.o,$(OBJS)) $(LDFLAGS) -o $@

# Run tests
test: $(TEST_BINS)
	@echo "Running tests..."
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo "All tests passed!"

# Install
install: release
	install -D $(TARGET) /usr/local/bin/ascii_cube

# Clean
clean:
	rm -rf $(BUILD_DIR)
	find . -name "*.gcda" -delete
	find . -name "*.gcno" -delete
	find . -name "*.gcov" -delete

# Help
help:
	@echo "Available targets:"
	@echo "  all      - Build debug version (default)"
	@echo "  debug    - Build with debug symbols and coverage"
	@echo "  release  - Build optimized release version"
	@echo "  test     - Build and run all tests"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  clean    - Remove build artifacts"
	@echo "  help     - Show this help message"
