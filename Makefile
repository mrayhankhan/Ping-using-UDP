# Makefile for UDP Ping Assignment
# Builds udpping and cping applications

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LDFLAGS = 

# Target executables
TARGETS = udpping cping

# Source files
UDPPING_SRC = udpping.c
CPING_SRC = cping.c

# Default target
all: $(TARGETS)

# Build udpping application
udpping: $(UDPPING_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build cping application
cping: $(CPING_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Clean build artifacts
clean:
	rm -f $(TARGETS)
	rm -f *.o
	rm -f core

# Install targets (optional)
install: $(TARGETS)
	@echo "Installing applications to /usr/local/bin (requires sudo)"
	sudo cp udpping /usr/local/bin/
	sudo cp cping /usr/local/bin/
	sudo chmod +x /usr/local/bin/udpping
	sudo chmod +x /usr/local/bin/cping

# Uninstall targets (optional)
uninstall:
	@echo "Removing applications from /usr/local/bin (requires sudo)"
	sudo rm -f /usr/local/bin/udpping
	sudo rm -f /usr/local/bin/cping

# Test target
test: udpping cping
	@echo "Built applications successfully"
	@echo "To test udpping: ./udpping <target_ip>"
	@echo "To test cping: sudo ./cping <target_ip>"

# Help target
help:
	@echo "Available targets:"
	@echo "  all       - Build both udpping and cping"
	@echo "  udpping   - Build UDP ping application"
	@echo "  cping     - Build ICMP ping application"
	@echo "  clean     - Remove built executables"
	@echo "  install   - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall - Remove from /usr/local/bin (requires sudo)"
	@echo "  test      - Build and show usage instructions"
	@echo "  help      - Show this help message"

# Debug build with symbols
debug: CFLAGS += -g -DDEBUG
debug: $(TARGETS)

# Static analysis
check: $(UDPPING_SRC) $(CPING_SRC)
	@echo "Running basic syntax check..."
	$(CC) $(CFLAGS) -fsyntax-only $(UDPPING_SRC)
	$(CC) $(CFLAGS) -fsyntax-only $(CPING_SRC)
	@echo "Syntax check passed"

# Show compiler and system info
info:
	@echo "Compiler: $(CC)"
	@echo "Flags: $(CFLAGS)"
	@echo "System: $(shell uname -a)"
	@echo "Architecture: $(shell uname -m)"

.PHONY: all clean install uninstall test help debug check info