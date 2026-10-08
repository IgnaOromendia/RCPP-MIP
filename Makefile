PYTHON ?= python3
OBJDIR ?= build
BIN ?= solverExec
PATH_BIN ?= pathSortExec
PATH_CLUSTER_BIN ?= pathSortClusterExec
BUILD_DIR := $(abspath $(OBJDIR))
SOLVER_BIN := $(abspath $(BIN))
PATH_SORT_BIN := $(abspath $(PATH_BIN))
PATH_SORT_CLUSTER_BIN := $(abspath $(PATH_CLUSTER_BIN))
COMMON_LIB := $(BUILD_DIR)/libpathsortcommon.a
FIRST_PASS_LIB := $(BUILD_DIR)/libpathsorterfirstpass.a
CLUSTERS_LIB := $(BUILD_DIR)/libpathsorterclusters.a

.PHONY: all mip path path-common path-first-pass path-clusters mip-tests \
        path-tests test test-unit clean

all: mip path-first-pass path-clusters

mip:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) all

path: path-first-pass

path-common:
	$(MAKE) -C mipPathSort OBJDIR=$(BUILD_DIR) LIB=$(COMMON_LIB) all

path-first-pass: path-common
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) COMMON_LIB=$(COMMON_LIB) all

path-clusters: path-common
	$(MAKE) -C mipPathSort-Clusters OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_CLUSTER_BIN) \
		LIB=$(CLUSTERS_LIB) \
		COMMON_LIB=$(COMMON_LIB) all

mip-tests:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) test-binaries

path-tests: path-common
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) COMMON_LIB=$(COMMON_LIB) test-binaries
	$(MAKE) -C mipPathSort-Clusters OBJDIR=$(BUILD_DIR) LIB=$(CLUSTERS_LIB) \
		BIN=$(PATH_SORT_CLUSTER_BIN) COMMON_LIB=$(COMMON_LIB) test-binaries

test-unit:
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) unit-test-binaries
	$(PYTHON) tests/run_tests.py --unit-only --build-dir $(OBJDIR)

test: all mip-tests path-tests
	$(PYTHON) tests/run_tests.py --build-dir $(OBJDIR) --solver $(BIN) \
		--path-sorter $(PATH_BIN) --path-cluster-sorter $(PATH_CLUSTER_BIN)

clean:
	$(MAKE) -C mipPathSort-Clusters OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_CLUSTER_BIN) \
		LIB=$(CLUSTERS_LIB) clean
	$(MAKE) -C mipPathSort-FirstPass OBJDIR=$(BUILD_DIR) BIN=$(PATH_SORT_BIN) \
		LIB=$(FIRST_PASS_LIB) clean
	$(MAKE) -C mipPathSort OBJDIR=$(BUILD_DIR) LIB=$(COMMON_LIB) clean
	$(MAKE) -C mipSolver OBJDIR=$(BUILD_DIR) BIN=$(SOLVER_BIN) clean
	$(RM) -r $(OBJDIR)
