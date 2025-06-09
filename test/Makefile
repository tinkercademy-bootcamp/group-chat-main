CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -g

SRCS := ../src
INCLUDE_PATHS := -I$(SRCS)/client \
                 -I$(SRCS)/net \
                 -I$(SRCS) \
                 -I./src

TARGET := chat_load_tester # to be updated
TEST_FRAMEWORK_SRC_DIR := ./perf

MAIN_PROJECT_CC_DEPS := $(TEST_FRAMEWORK_SRC_DIR)/client/chat-client.cc \
                        $(TEST_FRAMEWORK_SRC_DIR)/net/chat-sockets.cc \
                        $(TEST_FRAMEWORK_SRC_DIR)/utils.cc # Add utils.cc if it exists and is needed


TEST_FRAMEWORK_CC_FILES := $(wildcard $(TEST_FRAMEWORK_SRC_DIR)/*.cc)

# All source files to compile
ALL_SRCS_TO_COMPILE := $(TEST_FRAMEWORK_CC_FILES) $(MAIN_PROJECT_CC_DEPS)

all: $(TARGET)

$(TARGET): $(TEST_FRAMEWORK_SRC_DIR)/main.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) $^ -o $@

clean:
	rm -f $(TARGET) $(TEST_FRAMEWORK_SRC_DIR)/*.o
