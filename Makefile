# Makefile for CSS Checker
# Alternative to CMake for simple builds

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude
LDFLAGS = -pthread

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
BINDIR = $(BUILDDIR)/bin

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.cpp=$(BUILDDIR)/%.o)
TARGET = $(BINDIR)/css_checker

# Default target
all: $(TARGET)

# Create directories
$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Compile object files
$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Link executable
$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $@

# Clean build files
clean:
	rm -rf $(BUILDDIR)

# Install (optional)
install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

# Debug build
debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

# Release build (default optimization)
release: CXXFLAGS += -O3 -DNDEBUG
release: clean $(TARGET)

# Print variables (for debugging Makefile)
print-%:
	@echo $* = $($*)

.PHONY: all clean install debug release print-%

# Dependencies (simplified - in a real project you'd generate these automatically)
$(BUILDDIR)/main.o: $(INCDIR)/common.h $(INCDIR)/cli_parser.h $(INCDIR)/file_scanner.h $(INCDIR)/css_parser.h $(INCDIR)/usage_detector.h $(INCDIR)/report_generator.h
$(BUILDDIR)/common.o: $(INCDIR)/common.h
$(BUILDDIR)/cli_parser.o: $(INCDIR)/cli_parser.h $(INCDIR)/common.h
$(BUILDDIR)/css_parser.o: $(INCDIR)/css_parser.h $(INCDIR)/common.h
$(BUILDDIR)/file_scanner.o: $(INCDIR)/file_scanner.h $(INCDIR)/common.h
$(BUILDDIR)/usage_detector.o: $(INCDIR)/usage_detector.h $(INCDIR)/common.h
$(BUILDDIR)/report_generator.o: $(INCDIR)/report_generator.h $(INCDIR)/common.h $(INCDIR)/css_parser.h