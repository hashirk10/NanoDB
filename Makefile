# ============================================================
# NanoDB Makefile — CS-4002 Applied Programming
# ============================================================

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g
TARGET   := nanodb
RUNNER   := test_runner
SRCDIR   := src

# All .cpp files (recursive)
MAIN_SRC   := $(SRCDIR)/main.cpp
RUNNER_SRC := test_runner.cpp

# All header-only implementation (no separate .cpp files needed)
# since all logic is in .h files for this project

.PHONY: all clean run run_tests valgrind data help

all: $(TARGET) $(RUNNER)

$(TARGET): $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< -lm
	@echo "✓ Built $(TARGET)"

$(RUNNER): $(RUNNER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< -lm
	@echo "✓ Built $(RUNNER)"

run: $(TARGET)
	@mkdir -p data
	./$(TARGET)

run_tests: $(RUNNER)
	@mkdir -p data
	./$(RUNNER)

data:
	@echo "Generating TPC-H dataset..."
	python3 scripts/generate_data.py

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

clean:
	rm -f $(TARGET) $(RUNNER) nanodb_execution.log *.o

help:
	@echo ""
	@echo "  make            — Build nanodb and test_runner"
	@echo "  make data       — Generate TPC-H dataset (requires Python 3)"
	@echo "  make run        — Build and run nanodb (all 7 test cases)"
	@echo "  make run_tests  — Build and run automated test runner (queries.txt)"
	@echo "  make valgrind   — Run with Valgrind memory checker"
	@echo "  make clean      — Remove binaries and logs"
	@echo ""
