ROOT := ..

CXX = g++
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
                   -I$(ROOT)/mipBestPathSolver -I$(ROOT)/mipBestPathSolver/lib \
                   -I$(ROOT)/mipBestClusterPathSolver \
                   -I$(ROOT)/mipBestClusterPathSolver/lib -I$(ROOT)/tests
CPPFLAGS += -DNDEBUG -DIL_STD $(GLIB_CFLAGS) $(CPLEX_INC) $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
LDLIBS += $(GLIB_LIBS) -lm -lpthread

OBJDIR ?= $(ROOT)/build
BIN ?= $(ROOT)/bestClusterPathExec

COMMON_SRCS = $(wildcard $(ROOT)/cplexSolver/src/*.cpp \
              $(ROOT)/mipBestPathSolver/src/constraints/*.cpp \
              $(ROOT)/mipBestPathSolver/src/io/*.cpp \
              $(ROOT)/mipBestPathSolver/src/graph/*.cpp \
              $(ROOT)/mipBestPathSolver/src/model/*.cpp \
              $(ROOT)/mipBestPathSolver/src/util/*.cpp)
VARIANT_SRCS = $(wildcard $(ROOT)/mipBestClusterPathSolver/src/*.cpp \
               $(ROOT)/mipBestClusterPathSolver/src/io/*.cpp \
               $(ROOT)/mipBestClusterPathSolver/src/model/*.cpp)
SRCS = $(COMMON_SRCS) $(VARIANT_SRCS)
OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
CORE_OBJ = $(filter-out $(OBJDIR)/mipBestClusterPathSolver/src/main.o,$(OBJ))
TEST_OBJ = $(OBJDIR)/tests/best_cluster_path_solver_test.o
TEST_BIN = $(OBJDIR)/best_cluster_path_solver_test
DEPS = $(OBJ:.o=.d) $(TEST_OBJ:.o=.d)

.PHONY: all test-binaries clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(TEST_BIN): $(TEST_OBJ) $(CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(OBJDIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

test-binaries: $(TEST_BIN)

clean:
	$(RM) -r $(OBJDIR)/mipBestClusterPathSolver
	$(RM) $(TEST_OBJ) $(TEST_OBJ:.o=.d) $(TEST_BIN) $(BIN)

-include $(DEPS)
