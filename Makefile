# Makefile for Embedded Watchdog Framework

# Detect operating system
ifeq ($(OS),Windows_NT)
    CC = gcc
    CFLAGS = -Wall -Wextra -std=c99 -D_WIN32_WINNT=0x0600
    LDFLAGS = 
    EXT = .exe
else
    CC = gcc
    CFLAGS = -Wall -Wextra -std=c99 -pthread -D_GNU_SOURCE
    LDFLAGS = -pthread
    EXT = 
endif

# Source files
WATCHDOG_SOURCES = z_wdt.c watchdog_os.c
TEST_SOURCES = watchdog_test.c

# Object files
WATCHDOG_OBJECTS = $(WATCHDOG_SOURCES:.c=.o)
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Executables
TEST_TARGET = watchdog_test$(EXT)
LIBRARY_TARGET = libwatchdog.a

# Default target
all: $(LIBRARY_TARGET) $(TEST_TARGET)

# Build static library
$(LIBRARY_TARGET): $(WATCHDOG_OBJECTS)
	ar rcs $@ $^
	@echo "Built library: $@"

# Build test program
$(TEST_TARGET): $(TEST_OBJECTS) $(LIBRARY_TARGET)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Built test: $@"

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run tests
test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Clean build artifacts
clean:
	rm -f *.o $(LIBRARY_TARGET) $(TEST_TARGET)
	@echo "Cleaned build artifacts"

# Debug build
debug: CFLAGS += -g -DDEBUG -O0
debug: clean all

# Release build
release: CFLAGS += -O2 -DNDEBUG
release: clean all

# Show help
help:
	@echo "Available targets:"
	@echo "  all          - Build library, test (default)"
	@echo "  $(LIBRARY_TARGET) - Build static library only"
	@echo "  $(TEST_TARGET)    - Build test program"
	@echo "  test         - Run test program"
	@echo "  clean        - Remove build artifacts"
	@echo "  debug        - Build with debug symbols"
	@echo "  release      - Build optimized release version"
	@echo "  help         - Show this help message"

# Phony targets
.PHONY: all clean test debug release help
