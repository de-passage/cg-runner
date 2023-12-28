# Specify the compiler to use
CC = clang++

TARGET_EXEC=runner

# Specify the target executable
TARGET = $(basename $(notdir $(wildcard *.cpp)))

# Specify the source files
BUILD_DIR := ./build
SRC_DIRS := ./src

SRCS := $(shell find $(SRC_DIRS) -type f -name '*.cpp')

# Specify the object files
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

# As an example, ./build/hello.cpp.o turns into ./build/hello.cpp.d
DEPS := $(OBJS:.o=.d)

TARGET_PATH := $(BUILD_DIR)/$(TARGET_EXEC)

# Specify the include directories
INCLUDES =

# Specify the library directories
LIBRARIES =

# Specify the libraries to link with
LIBS =

# Specify the compile flags
CXXFLAGS =

ACTUAL_CFLAGS = -MMD -MP $(CXXFLAGS) $(shell cat compile_flags.txt)

# Specify the linker flags
LDFLAGS =

.PHONY: all clean

build: $(BUILD_DIR)/$(TARGET_EXEC)

# The final build step.
$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CXX) $(OBJS) -o $@ $(LDFLAGS)

# Build step for C source
$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Build step for C++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(ACTUAL_CFLAGS) -c $< -o $@

# Target for cleaning up
clean:
	rm -r $(BUILD_DIR)

run: $(BUILD_DIR)/$(TARGET_EXEC)
	$(BUILD_DIR)/$(TARGET_EXEC) $(ARGS)

# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want those
# errors to show up.
-include $(DEPS)
