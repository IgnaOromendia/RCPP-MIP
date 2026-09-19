CXX = g++
PKG_CONFIG ?= pkg-config
PYTHON ?= python3

CPLEX_DIR ?= /Applications/CPLEX_Studio2211
CPLEX_PLATFORM ?= arm64_osx
CPLEX_INC = -I$(CPLEX_DIR)/cplex/include -I$(CPLEX_DIR)/concert/include
CPLEX_LIB = -L$(CPLEX_DIR)/cplex/lib/$(CPLEX_PLATFORM)/static_pic \
            -L$(CPLEX_DIR)/concert/lib/$(CPLEX_PLATFORM)/static_pic

GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0)
GLIB_LIBS = $(shell $(PKG_CONFIG) --libs glib-2.0)

CPPFLAGS += -DNDEBUG $(GLIB_CFLAGS) -ImipSolver -ImipSolver/lib
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
CPLEX_LDLIBS = -lilocplex -lconcert -lcplex
LDLIBS += $(GLIB_LIBS) -lm -lpthread

OBJDIR = build
SRCS = $(wildcard mipSolver/src/*.cpp mipSolver/src/constraints/*.cpp mipSolver/src/io/*.cpp)
OBJ = $(addprefix $(OBJDIR)/,$(SRCS:.cpp=.o))
DEPS = $(OBJ:.o=.d)
TEST_OBJ = $(OBJDIR)/tests/solver_result_test.o
DEPS += $(TEST_OBJ:.o=.d)
TEST_BIN = $(OBJDIR)/solver_result_test
LIFETIME_TEST_OBJ = $(OBJDIR)/tests/solver_lifetime_test.o
LIFETIME_TEST_BIN = $(OBJDIR)/solver_lifetime_test
DEPS += $(LIFETIME_TEST_OBJ:.o=.d)
CONSTRAINT_TEST_OBJ = $(OBJDIR)/tests/constraint_setter_test.o
CONSTRAINT_TEST_BIN = $(OBJDIR)/constraint_setter_test
DEPS += $(CONSTRAINT_TEST_OBJ:.o=.d)
STRUCTURE_TESTS = graph_test super_graph_test instance_reader_test solution_writer_test cli_options_test
STRUCTURE_TEST_BINS = $(addprefix $(OBJDIR)/,$(STRUCTURE_TESTS))
STRUCTURE_TEST_OBJ = $(addprefix $(OBJDIR)/tests/,$(addsuffix .o,$(STRUCTURE_TESTS)))
DEPS += $(STRUCTURE_TEST_OBJ:.o=.d)
BIN = solverExec

.PHONY: all clean test test-unit

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $(OBJ) $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(OBJDIR)/mipSolver/src/main.o $(OBJDIR)/mipSolver/src/RCPPSolver.o $(OBJDIR)/mipSolver/src/CPLEXSolver.o $(OBJDIR)/mipSolver/src/FOSolver.o $(TEST_OBJ) $(LIFETIME_TEST_OBJ) $(OBJDIR)/tests/solver_options_test.o: CPPFLAGS += -DIL_STD $(CPLEX_INC)

$(filter $(OBJDIR)/mipSolver/src/constraints/%,$(OBJ)) $(CONSTRAINT_TEST_OBJ): CPPFLAGS += -DIL_STD $(CPLEX_INC)

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	$(RM) -r $(OBJDIR) $(BIN)

$(TEST_BIN): $(TEST_OBJ) $(filter-out $(OBJDIR)/mipSolver/src/main.o,$(OBJ))
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(LIFETIME_TEST_BIN): $(LIFETIME_TEST_OBJ) $(filter-out $(OBJDIR)/mipSolver/src/main.o,$(OBJ))
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(CONSTRAINT_TEST_BIN): $(CONSTRAINT_TEST_OBJ) $(filter-out $(OBJDIR)/mipSolver/src/main.o,$(OBJ))
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(OBJDIR)/graph_test: $(OBJDIR)/tests/graph_test.o $(addprefix $(OBJDIR)/mipSolver/src/,Graph.o Instance.o io/InstanceReader.o)
	$(CXX) $^ $(GLIB_LIBS) -o $@

$(OBJDIR)/super_graph_test: $(OBJDIR)/tests/super_graph_test.o $(addprefix $(OBJDIR)/mipSolver/src/,Graph.o SuperGraph.o HashMap.o Instance.o io/InstanceReader.o)
	$(CXX) $^ $(GLIB_LIBS) -o $@

$(OBJDIR)/instance_reader_test: $(OBJDIR)/tests/instance_reader_test.o $(addprefix $(OBJDIR)/mipSolver/src/,Instance.o io/InstanceReader.o Graph.o)
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJDIR)/solution_writer_test: $(OBJDIR)/tests/solution_writer_test.o $(OBJDIR)/mipSolver/src/io/SolutionWriter.o
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJDIR)/cli_options_test: $(OBJDIR)/tests/cli_options_test.o $(OBJDIR)/mipSolver/src/CliOptions.o
	$(CXX) $(LDFLAGS) $^ -o $@

$(OBJDIR)/solver_options_test: $(OBJDIR)/tests/solver_options_test.o $(filter-out $(OBJDIR)/mipSolver/src/main.o,$(OBJ))
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

DEPS += $(OBJDIR)/tests/solver_options_test.d

test-unit: $(STRUCTURE_TEST_BINS)
	$(PYTHON) tests/run_tests.py --unit-only --build-dir $(OBJDIR)

test: $(BIN) $(TEST_BIN) $(LIFETIME_TEST_BIN) $(STRUCTURE_TEST_BINS) $(OBJDIR)/solver_options_test $(CONSTRAINT_TEST_BIN)
	$(PYTHON) tests/run_tests.py --build-dir $(OBJDIR) --solver $(BIN)

-include $(DEPS)
