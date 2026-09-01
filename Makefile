CXX ?= g++
CXXFLAGS = -std=c++14 -O2 -Wall -Wextra -Iinclude

ifeq ($(OS),Windows_NT)
    PLATFORM = win
    EXE = .exe
    MKDIR = if not exist $(subst /,\,$(1)) mkdir $(subst /,\,$(1))
    RMDIR = if exist $(subst /,\,$(1)) rmdir /s /q $(subst /,\,$(1))
else
    PLATFORM = linux
    EXE =
    MKDIR = mkdir -p $(1)
    RMDIR = rm -rf $(1)
endif

BIN_DIR = bin/$(PLATFORM)
OBJ_DIR = obj/$(PLATFORM)

OBJS = $(OBJ_DIR)/allocator.o $(OBJ_DIR)/strategy.o $(OBJ_DIR)/diagnostics.o $(OBJ_DIR)/embedded_simulator.o

TARGET_CLI = $(BIN_DIR)/demo_runner$(EXE)
TARGET_TEST = $(BIN_DIR)/run_tests$(EXE)
TARGET_BENCH = $(BIN_DIR)/run_benchmarks$(EXE)

all: dirs $(TARGET_CLI) $(TARGET_TEST) $(TARGET_BENCH)

dirs:
	@$(call MKDIR,$(BIN_DIR))
	@$(call MKDIR,$(OBJ_DIR))

$(OBJ_DIR)/%.o: src/%.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/test_allocator.o: tests/test_allocator.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)/benchmark.o: benchmarks/benchmark.cpp | dirs
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET_CLI): $(OBJS) $(OBJ_DIR)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TARGET_TEST): $(OBJS) $(OBJ_DIR)/test_allocator.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TARGET_BENCH): $(OBJS) $(OBJ_DIR)/benchmark.o
	$(CXX) $(CXXFLAGS) $^ -o $@

cli: dirs $(TARGET_CLI)
	$(TARGET_CLI)

embedded-demo: dirs $(TARGET_CLI)
	$(TARGET_CLI) --embedded

embedded-comp: dirs $(TARGET_CLI)
	$(TARGET_CLI) --comp

test: dirs $(TARGET_TEST)
	$(TARGET_TEST)

bench: dirs $(TARGET_BENCH)
	$(TARGET_BENCH)

clean:
	@$(call RMDIR,obj)
	@$(call RMDIR,bin)

.PHONY: all dirs cli embedded-demo embedded-comp test bench clean
