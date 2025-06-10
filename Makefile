CXX := g++

CXXFLAGS := -std=c++20 -Wall -Wextra -pedantic -fsanitize=address
CXX_DEBUG_FLAGS := -g3 -ggdb3
CXX_RELEASE_FLAGS := -O3
CXXFLAGS += -O2 -g -fno-omit-frame-pointer
CXXFLAGS += $(CXX_DEBUG_FLAGS)

LDFLAGS := -fsanitize=address
LIBS := fmt spdlog
LIB_FLAGS := $(addprefix -l,$(LIBS))
LDFLAGS += $(LIB_FLAGS) -pthread -lgtest -lgtest_main

BUILD_DIR := build
SRC_DIR := src

TEST_DIR := test/correctness
TEST_BUILD_DIR := $(TEST_DIR)/build
TEST_SRC := $(TEST_DIR)/test_command_create.cc
TEST_OBJS := $(TEST_BUILD_DIR)/test_command_create.cc.o
TEST_TARGET := $(TEST_BUILD_DIR)/command-create-test

# Project source files
SRCS := $(shell find $(SRC_DIR) -type f -name '*.cc')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
NON_MAIN_OBJS := $(filter-out %main.cc.o, $(OBJS))
DEPS := $(OBJS:.o=.d)

# Include flags
INC_DIRS := $(shell find $(SRC_DIR) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS))
CPPFLAGS := $(INC_FLAGS) -MMD -MP

# Build everything
all: $(BUILD_DIR)/server $(BUILD_DIR)/client $(TEST_TARGET)

# Build server
$(BUILD_DIR)/server: $(BUILD_DIR)/src/server-main.cc.o $(NON_MAIN_OBJS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Build client
$(BUILD_DIR)/client: $(BUILD_DIR)/src/client-main.cc.o $(NON_MAIN_OBJS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

# Compile main project .cc to .o
$(BUILD_DIR)/%.cc.o: %.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Compile test source to object
$(TEST_BUILD_DIR)/%.cc.o: $(TEST_DIR)/%.cc
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Build test binary
$(TEST_TARGET): $(TEST_OBJS) $(NON_MAIN_OBJS)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(INC_FLAGS) $^ -o $@ $(LDFLAGS)

# Run test
.PHONY: run-test
run-test: $(TEST_TARGET)
	./$(TEST_TARGET)

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TEST_BUILD_DIR)

# Debug info
.PHONY: print-vars
print-vars:
	@echo "SRCS=$(SRCS)"
	@echo "OBJS=$(OBJS)"
	@echo "NON_MAIN_OBJS=$(NON_MAIN_OBJS)"
	@echo "DEPS=$(DEPS)"
	@echo "INC_FLAGS=$(INC_FLAGS)"

# Flamegraph
.PHONY: setup-flamegraph
setup-flamegraph:
	mkdir -p external-tools/
	if [ ! -d external-tools/FlameGraph ]; then git clone https://github.com/brendangregg/FlameGraph.git external-tools/FlameGraph; fi
	chmod +x auto_profiler.sh

.PHONY: flamegraph
flamegraph: all
	./auto_profiler.sh

-include $(DEPS)
