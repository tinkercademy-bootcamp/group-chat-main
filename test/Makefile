CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pthread -g

SRCS := ../src
INCLUDE_PATHS := -I$(SRCS)/client \
                 -I$(SRCS)/net \
                 -I$(SRCS) \
                 -I./src

TARGET := chat_load_tester # to be updated
TEST_FRAMEWORK_SRC_DIR := ./src

all: $(TARGET)

$(TARGET): $(TEST_FRAMEWORK_SRC_DIR)/main_tester.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_PATHS) $^ -o $@

clean:
	rm -f $(TARGET) $(TEST_FRAMEWORK_SRC_DIR)/*.o