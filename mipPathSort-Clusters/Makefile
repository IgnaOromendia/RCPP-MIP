ROOT := ..

CXX = g++
AR ?= ar
PKG_CONFIG ?= pkg-config

CPLEX_DIR ?= /Applications/CPLEX_Studio2211
CPLEX_PLATFORM ?= arm64_osx
CPLEX_INC = -isystem $(CPLEX_DIR)/cplex/include \
            -isystem $(CPLEX_DIR)/concert/include
CPLEX_LIB = -L$(CPLEX_DIR)/cplex/lib/$(CPLEX_PLATFORM)/static_pic \
            -L$(CPLEX_DIR)/concert/lib/$(CPLEX_PLATFORM)/static_pic
CPLEX_LDLIBS = -lilocplex -lconcert -lcplex

GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0)
GLIB_LIBS = $(shell $(PKG_CONFIG) --libs glib-2.0)

PROJECT_INCLUDES = -I$(ROOT)/cplexSolver -I$(ROOT)/cplexSolver/lib \
                   -I$(ROOT)/mipSolver -I$(ROOT)/mipSolver/lib \
                   -I$(ROOT)/mipPathSort/lib \
                   -I$(ROOT)/mipPathSort-Clusters/lib
CPPFLAGS += -DNDEBUG -DIL_STD $(GLIB_CFLAGS) $(CPLEX_INC) \
            $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
LDLIBS += $(GLIB_LIBS) -lm -lpthread

OBJDIR ?= $(ROOT)/build
LIB ?= $(OBJDIR)/libpathsorterclusters.a
COMMON_LIB ?= $(OBJDIR)/libpathsortcommon.a
COMMON_INPUTS = $(wildcard $(ROOT)/mipPathSort/lib/constraints/*.h \
                $(ROOT)/mipPathSort/lib/io/*.h \
                $(ROOT)/mipPathSort/lib/model/*.h \
                $(ROOT)/mipPathSort/src/constraints/*.cpp \
                $(ROOT)/mipPathSort/src/io/*.cpp \
                $(ROOT)/mipPathSort/src/model/*.cpp) \
                $(ROOT)/mipPathSort/Makefile

CLUSTERS_SRCS = $(ROOT)/mipPathSort-Clusters/src/constraints/ClusterConstraintSetter.cpp \
                $(ROOT)/mipPathSort-Clusters/src/model/PathSorterClusters.cpp \
                $(ROOT)/mipPathSort-Clusters/src/io/ClusterPathSortInstanceReader.cpp
CLUSTERS_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(CLUSTERS_SRCS))
SUPPORT_SRCS = $(ROOT)/cplexSolver/src/CPLEXSolver.cpp \
               $(ROOT)/mipSolver/src/graph/Graph.cpp \
               $(ROOT)/mipSolver/src/graph/SuperGraph.cpp \
               $(ROOT)/mipSolver/src/io/InstanceReader.cpp \
               $(ROOT)/mipSolver/src/model/Instance.cpp \
               $(ROOT)/mipSolver/src/util/HashMap.cpp
SUPPORT_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(SUPPORT_SRCS))
CLUSTERS_TEST_OBJ = $(OBJDIR)/tests/path_sorter_clusters_test.o
CLUSTERS_TEST_BIN = $(OBJDIR)/path_sorter_clusters_test
MAIN_OBJ = $(OBJDIR)/mipPathSort-Clusters/src/main.o
BIN ?= $(ROOT)/pathSortClusterExec
DEPS = $(CLUSTERS_OBJ:.o=.d) $(SUPPORT_OBJ:.o=.d) $(CLUSTERS_TEST_OBJ:.o=.d) \
       $(MAIN_OBJ:.o=.d)

.PHONY: all clean test-binaries

all: $(COMMON_LIB) $(LIB) $(BIN)

$(COMMON_LIB): $(COMMON_INPUTS)
	$(MAKE) -C $(ROOT)/mipPathSort OBJDIR=$(abspath $(OBJDIR)) \
		LIB=$(abspath $(COMMON_LIB)) all

$(LIB): $(CLUSTERS_OBJ) Makefile
	mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $(CLUSTERS_OBJ)

$(BIN): $(MAIN_OBJ) $(CLUSTERS_OBJ) $(COMMON_LIB) $(SUPPORT_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(OBJDIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(CLUSTERS_TEST_BIN): $(CLUSTERS_TEST_OBJ) $(CLUSTERS_OBJ) $(COMMON_LIB) $(SUPPORT_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

test-binaries: $(CLUSTERS_TEST_BIN)

clean:
	$(RM) -r $(OBJDIR)/mipPathSort-Clusters
	$(RM) $(LIB) $(CLUSTERS_TEST_OBJ) $(CLUSTERS_TEST_OBJ:.o=.d) $(CLUSTERS_TEST_BIN) \
	      $(MAIN_OBJ) $(BIN)

-include $(DEPS)
