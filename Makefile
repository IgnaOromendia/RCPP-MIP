PYTHON ?= python3
OBJDIR ?= build
BIN ?= solverExec
PATH_BIN ?= pathSortExec
BUILD_DIR := $(abspath $(OBJDIR))
SOLVER_BIN := $(abspath $(BIN))
PATH_SORT_BIN := $(abspath $(PATH_BIN))
FIRST_PASS_LIB := $(BUILD_DIR)/libpathsorterfirstpass.a
CLUSTERS_LIB := $(BUILD_DIR)/libpathsorterclusters.a

.PHONY: all mip path path-first-pass path-clusters mip-tests \
        path-tests test test-unit clean

all: mip path-first-pass path-clusters

mip:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) all

path: path-first-pass

path-first-pass:
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) all

path-clusters:
	$(MAKE) -C mipPathSort-Clusters OBJDIR=$(BUILD_DIR) LIB=$(CLUSTERS_LIB) all

mip-tests:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) test-binaries

path-tests:
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) test-binaries
	$(MAKE) -C mipPathSort-Clusters OBJDIR=$(BUILD_DIR) LIB=$(CLUSTERS_LIB) test-binaries

test-unit:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) unit-test-binaries
	$(PYTHON) tests/run_tests.py --unit-only --build-dir $(OBJDIR)

test: all mip-tests path-tests
	$(PYTHON) tests/run_tests.py --build-dir $(OBJDIR) --solver $(BIN) \
		--path-sorter $(PATH_BIN)

clean:
	$(MAKE) -C mipPathSort-Clusters OBJDIR=$(BUILD_DIR) LIB=$(CLUSTERS_LIB) clean
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) clean
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) clean
	$(RM) -r $(OBJDIR)
