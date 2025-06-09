CXX := g++
CXXFLAGS := -std=c++23 -Wall -Wextra -pthread -g

SRCS := ../src
INCLUDE_PATHS := -I$(SRCS)/client \
                 -I$(SRCS)/net \
                 -I$(SRCS) \
                 -I./src

TARGET := chat_load_tester # to be updated
TEST_FRAMEWORK_SRC_DIR := ./perf

MAIN_PROJECT_CC_DEPS := $(SRCS)/client/chat-client.cc \
                        $(SRCS)/net/chat-sockets.cc \
                        $(SRCS)/utils.h 


TEST_FRAMEWORK_CC_FILES := $(wildcard $(TEST_FRAMEWORK_SRC_DIR)/*.cc)

# All source files to compile
ALL_SRCS_TO_COMPILE := $(TEST_FRAMEWORK_CC_FILES) $(MAIN_PROJECT_CC_DEPS)

all: $(TARGET)

$(TARGET): $(ALL_SRCS_TO_COMPILE)
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) $(ALL_SRCS_TO_COMPILE) -o $@ -lfmt

clean:
	rm -f $(TARGET) $(TEST_FRAMEWORK_SRC_DIR)/*.o
