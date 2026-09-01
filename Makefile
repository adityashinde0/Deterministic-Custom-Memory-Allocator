CXX ?= g++
CXXFLAGS = -std=c++14 -O2 -Wall -Wextra -Iinclude

SRCS = src/allocator.cpp src/strategy.cpp src/diagnostics.cpp
OBJS = obj/allocator.o obj/strategy.o obj/diagnostics.o

BIN_DIR = bin
OBJ_DIR = obj

ifeq ($(OS),Windows_NT)
    EXE = .exe
    MKDIR = if not exist $(1) mkdir $(1)
    RMDIR = if exist $(1) rmdir /s /q $(1)
else
    EXE =
    MKDIR = mkdir -p $(1)
    RMDIR = rm -rf $(1)
endif

TARGET_CLI = $(BIN_DIR)/demo_runner$(EXE)
TARGET_TEST = $(BIN_DIR)/test_runner$(EXE)
TARGET_BENCH = $(BIN_DIR)/benchmark_runner$(EXE)

all: dirs $(TARGET_CLI) $(TARGET_TEST) $(TARGET_BENCH)

dirs:
	@$(call MKDIR,$(BIN_DIR))
	@$(call MKDIR,$(OBJ_DIR))

$(OBJ_DIR)/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET_CLI): $(OBJS) $(OBJ_DIR)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/main.o: src/main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET_TEST): $(OBJS) $(OBJ_DIR)/test_allocator.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/test_allocator.o: tests/test_allocator.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET_BENCH): $(OBJS) $(OBJ_DIR)/benchmark.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(OBJ_DIR)/benchmark.o: benchmarks/benchmark.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

cli: dirs $(TARGET_CLI)
	$(TARGET_CLI)

test: dirs $(TARGET_TEST)
	$(TARGET_TEST)

bench: dirs $(TARGET_BENCH)
	$(TARGET_BENCH)

clean:
	@$(call RMDIR,$(OBJ_DIR))
	@$(call RMDIR,$(BIN_DIR))

.PHONY: all dirs cli test bench clean
