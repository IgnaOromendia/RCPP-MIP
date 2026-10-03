PYTHON ?= python3
OBJDIR ?= build
BIN ?= solverExec
BUILD_DIR := $(abspath $(OBJDIR))
SOLVER_BIN := $(abspath $(BIN))

.PHONY: all mip path mip-tests path-tests test test-unit clean

all:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) all
	$(MAKE) -C mipPathSort OBJDIR=$(BUILD_DIR) all

mip:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) all

path:
	$(MAKE) -C mipPathSort OBJDIR=$(BUILD_DIR) all

mip-tests:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) test-binaries

path-tests:
	$(MAKE) -C mipPathSort OBJDIR=$(BUILD_DIR) test-binaries

test-unit:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) unit-test-binaries
	$(PYTHON) tests/run_tests.py --unit-only --build-dir $(OBJDIR)

test: all mip-tests path-tests
	$(PYTHON) tests/run_tests.py --build-dir $(OBJDIR) --solver $(BIN)

clean:
	$(MAKE) -C mipPathSort OBJDIR=$(BUILD_DIR) clean
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) clean
	$(RM) -r $(OBJDIR)
