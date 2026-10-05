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
                   -I$(ROOT)/mipSolver -I$(ROOT)/mipSolver/lib \
                   -I$(ROOT)/mipPathSort -I$(ROOT)/mipPathSort/lib \
                   -I$(ROOT)/tests
CPPFLAGS += -DNDEBUG $(GLIB_CFLAGS) $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
LDLIBS += $(GLIB_LIBS) -lm -lpthread

OBJDIR ?= $(ROOT)/build
BIN ?= $(ROOT)/solverExec

SRCS = $(wildcard $(ROOT)/cplexSolver/src/*.cpp \
       $(ROOT)/mipSolver/src/*.cpp \
       $(ROOT)/mipSolver/src/constraints/*.cpp \
       $(ROOT)/mipSolver/src/io/*.cpp \
       $(ROOT)/mipSolver/src/graph/*.cpp \
       $(ROOT)/mipSolver/src/model/*.cpp \
       $(ROOT)/mipSolver/src/heuristic/*.cpp \
       $(ROOT)/mipSolver/src/util/*.cpp)
OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(SRCS))
CORE_OBJ = $(filter-out $(OBJDIR)/mipSolver/src/main.o,$(OBJ))

SOLVER_RESULT_OBJ = $(OBJDIR)/tests/solver_result_test.o
SOLVER_RESULT_BIN = $(OBJDIR)/solver_result_test
CONSTRAINT_OBJ = $(OBJDIR)/tests/constraint_setter_test.o
CONSTRAINT_BIN = $(OBJDIR)/constraint_setter_test
SOLVER_OPTIONS_OBJ = $(OBJDIR)/tests/solver_options_test.o
SOLVER_OPTIONS_BIN = $(OBJDIR)/solver_options_test

STRUCTURE_TESTS = graph_test super_graph_test instance_reader_test \
                  solution_writer_test cli_options_test fix_and_optimize_test
STRUCTURE_BINS = $(addprefix $(OBJDIR)/,$(STRUCTURE_TESTS))
STRUCTURE_OBJ = $(addprefix $(OBJDIR)/tests/,$(addsuffix .o,$(STRUCTURE_TESTS)))

DEPS = $(OBJ:.o=.d) $(SOLVER_RESULT_OBJ:.o=.d) $(CONSTRAINT_OBJ:.o=.d) \
       $(SOLVER_OPTIONS_OBJ:.o=.d) $(STRUCTURE_OBJ:.o=.d)

.PHONY: all clean test-binaries unit-test-binaries

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

CPLEX_OBJECTS = $(OBJDIR)/cplexSolver/src/CPLEXSolver.o \
                $(OBJDIR)/mipSolver/src/main.o \
                $(OBJDIR)/mipSolver/src/model/RCPPSolver.o \
                $(OBJDIR)/mipSolver/src/heuristic/FixAndOptimize.o \
                $(SOLVER_RESULT_OBJ) $(SOLVER_OPTIONS_OBJ)
$(CPLEX_OBJECTS): CPPFLAGS += -DIL_STD $(CPLEX_INC)

$(filter $(OBJDIR)/mipSolver/src/constraints/%,$(OBJ)) $(CONSTRAINT_OBJ): CPPFLAGS += -DIL_STD $(CPLEX_INC)

$(OBJDIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(SOLVER_RESULT_BIN): $(SOLVER_RESULT_OBJ) $(CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(CONSTRAINT_BIN): $(CONSTRAINT_OBJ) $(CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(SOLVER_OPTIONS_BIN): $(SOLVER_OPTIONS_OBJ) $(CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(OBJDIR)/graph_test: $(OBJDIR)/tests/graph_test.o $(addprefix $(OBJDIR)/mipSolver/src/,graph/Graph.o model/Instance.o io/InstanceReader.o)
	$(CXX) $^ $(GLIB_LIBS) -o $@

$(OBJDIR)/super_graph_test: $(OBJDIR)/tests/super_graph_test.o $(addprefix $(OBJDIR)/mipSolver/src/,graph/Graph.o graph/SuperGraph.o util/HashMap.o model/Instance.o io/InstanceReader.o)
	$(CXX) $^ $(GLIB_LIBS) -o $@

$(OBJDIR)/instance_reader_test: $(OBJDIR)/tests/instance_reader_test.o $(addprefix $(OBJDIR)/mipSolver/src/,model/Instance.o io/InstanceReader.o graph/Graph.o)
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJDIR)/solution_writer_test: $(OBJDIR)/tests/solution_writer_test.o $(OBJDIR)/mipSolver/src/io/SolutionWriter.o
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJDIR)/cli_options_test: $(OBJDIR)/tests/cli_options_test.o $(OBJDIR)/mipSolver/src/util/CliOptions.o
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJDIR)/fix_and_optimize_test: $(OBJDIR)/tests/fix_and_optimize_test.o $(addprefix $(OBJDIR)/mipSolver/src/,graph/Graph.o graph/SuperGraph.o util/HashMap.o model/Instance.o io/InstanceReader.o)
	$(CXX) $(LDFLAGS) $^ $(GLIB_LIBS) -o $@

unit-test-binaries: $(STRUCTURE_BINS)

test-binaries: $(BIN) $(SOLVER_RESULT_BIN) $(CONSTRAINT_BIN) \
               $(SOLVER_OPTIONS_BIN) $(STRUCTURE_BINS)

clean:
	$(RM) -r $(OBJDIR)/cplexSolver $(OBJDIR)/mipSolver
	$(RM) $(SOLVER_RESULT_OBJ) $(SOLVER_RESULT_OBJ:.o=.d) \
	      $(CONSTRAINT_OBJ) $(CONSTRAINT_OBJ:.o=.d) \
	      $(SOLVER_OPTIONS_OBJ) $(SOLVER_OPTIONS_OBJ:.o=.d) \
	      $(STRUCTURE_OBJ) $(STRUCTURE_OBJ:.o=.d)
	$(RM) $(BIN) $(SOLVER_RESULT_BIN) $(CONSTRAINT_BIN) \
	      $(SOLVER_OPTIONS_BIN) $(STRUCTURE_BINS)

-include $(DEPS)
