PYTHON ?= python3
OBJDIR ?= build
BIN ?= solverExec
PATH_BIN ?= pathSortExec
BUILD_DIR := $(abspath $(OBJDIR))
SOLVER_BIN := $(abspath $(BIN))
PATH_SORT_BIN := $(abspath $(PATH_BIN))
COMMON_LIB := $(BUILD_DIR)/libpathsortcommon.a
FIRST_PASS_LIB := $(BUILD_DIR)/libpathsorterfirstpass.a
REGION_LIB := $(BUILD_DIR)/libpathsorterregion.a

.PHONY: all mip path path-common path-first-pass path-region mip-tests \
        path-tests test test-unit clean

all: mip path-first-pass path-region

mip:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) all

path: path-first-pass

path-common:
	$(MAKE) -C mipPathSort-Common OBJDIR=$(BUILD_DIR) LIB=$(COMMON_LIB) all

path-first-pass: path-common
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) COMMON_LIB=$(COMMON_LIB) all

path-region: path-common
	$(MAKE) -C mipPathSort-Region OBJDIR=$(BUILD_DIR) LIB=$(REGION_LIB) \
		COMMON_LIB=$(COMMON_LIB) all

mip-tests:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) test-binaries

path-tests: path-common
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) COMMON_LIB=$(COMMON_LIB) test-binaries
	$(MAKE) -C mipPathSort-Region OBJDIR=$(BUILD_DIR) LIB=$(REGION_LIB) \
		COMMON_LIB=$(COMMON_LIB) test-binaries

test-unit:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) unit-test-binaries
	$(PYTHON) tests/run_tests.py --unit-only --build-dir $(OBJDIR)

test: all mip-tests path-tests
	$(PYTHON) tests/run_tests.py --build-dir $(OBJDIR) --solver $(BIN) \
		--path-sorter $(PATH_BIN)

clean:
	$(MAKE) -C mipPathSort-Region OBJDIR=$(BUILD_DIR) LIB=$(REGION_LIB) clean
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) clean
	$(MAKE) -C mipPathSort-Common OBJDIR=$(BUILD_DIR) LIB=$(COMMON_LIB) clean
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) clean
	$(RM) -r $(OBJDIR)
