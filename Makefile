# Thanks to Job Vranish (https://spin.atomicobject.com/2016/08/26/makefile-c-projects/)
SHELL := /bin/bash
# define the C/C++ compiler to use
CXX := g++

# define the C/C++ compiler flags
CXXFLAGS :=-std=c++20 -Wall -Wextra -pedantic -fsanitize=address

CXX_DEBUG_FLAGS :=-g3 -ggdb3
CXX_RELEASE_FLAGS :=-O3

# PASS -DIO_URING_ENABLED to use IO_URING instead of EPOLL
CXXFLAGS += -O0 -g -fno-omit-frame-pointer -DIO_URING_ENABLED

CXXFLAGS += $(CXX_DEBUG_FLAGS)



# The linker flags. These are passed to the linker when we link our object files together.
LDFLAGS := -fsanitize=address

LIBS:= fmt spdlog uring
LIB_FLAGS := $(addprefix -l,$(LIBS))
LDFLAGS += $(LIB_FLAGS) -pthread

BUILD_DIR := build
SRC_DIR := src

# Find all the C and C++ files we want to compile
# Note the single quotes around the * expressions. The shell will incorrectly expand these otherwise, but we want to send the * directly to the find command.

SRCS := $(shell find $(SRC_DIR) -type f -name '*.cc')

# Prepends BUILD_DIR and appends .o to every src file
# As an example, ./your_dir/hello.cpp turns into ./build/./your_dir/hello.cpp.o
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
NON_MAIN_OBJS := $(filter-out %main.cc.o, $(OBJS))

# String substitution (suffix version without %).
# As an example, ./build/hello.cpp.o turns into ./build/hello.cpp.d
DEPS := $(OBJS:.o=.d)

# Every folder in ./src will need to be passed to GCC so that it can find header files
INC_DIRS := $(shell find $(SRC_DIR) -type d)
# Add a prefix to INC_DIRS. So moduleA would become -ImoduleA. GCC understands this -I flag
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

# The -MMD and -MP flags together generate Makefiles for us!
# See https://scottmcpeak.com/autodepend/autodepend.html for more information.
# These files will have .d instead of .o as the output.
CPPFLAGS := $(INC_FLAGS) -MMD -MP

all: $(BUILD_DIR)/server $(BUILD_DIR)/client
	
$(BUILD_DIR)/server: $(BUILD_DIR)/src/server-main.cc.o $(NON_MAIN_OBJS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BUILD_DIR)/src/server-main.cc.o $(NON_MAIN_OBJS) -o $(BUILD_DIR)/server $(LDFLAGS)

$(BUILD_DIR)/client: $(BUILD_DIR)/src/client-main.cc.o $(NON_MAIN_OBJS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BUILD_DIR)/src/client-main.cc.o $(NON_MAIN_OBJS) -o $(BUILD_DIR)/client $(LDFLAGS)

$(BUILD_DIR)/%.cc.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.PHONY: print-vars
print-vars:
	@echo "SRCS=$(SRCS)"
	@echo "OBJS=$(OBJS)"
	@echo "NON_MAIN_OBJS=$(NON_MAIN_OBJS)"
	@echo "DEPS=$(DEPS)"
	@echo "INC_FLAGS=$(INC_FLAGS)"

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: setup-flamegraph
setup-flamegraph: all
	mkdir -p external-tools/
	if [ ! -d external-tools/FlameGraph ]; then git clone https://github.com/brendangregg/FlameGraph.git external-tools/FlameGraph; fi
	chmod +x auto_profiler.sh
	
.PHONY: flamegraph
flamegraph: all
	./auto_profiler.sh	

.PHONY: stress
stress: all
	./auto_profiler.sh --auto
	
# Include the .d makefiles. The - at the front suppresses the errors of missing
# Makefiles. Initially, all the .d files will be missing, and we don't want those
# errors to show up.
-include $(DEPS)