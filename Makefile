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

CPPFLAGS += -DNDEBUG -DIL_STD $(CPLEX_INC) $(GLIB_CFLAGS) -ImipSolver -ImipSolver/lib
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
LDFLAGS += $(CPLEX_LIB)
LDLIBS += -lilocplex -lconcert -lcplex $(GLIB_LIBS) -lm -lpthread

OBJDIR = build
SRCS = main.cpp $(wildcard mipSolver/src/*.cpp)
OBJ = $(addprefix $(OBJDIR)/,$(SRCS:.cpp=.o))
DEPS = $(OBJ:.o=.d)
TEST_OBJ = $(OBJDIR)/tests/solver_result_test.o
DEPS += $(TEST_OBJ:.o=.d)
TEST_BIN = $(OBJDIR)/solver_result_test
BIN = solverExec

.PHONY: all clean test

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(LDFLAGS) $(OBJ) $(LDLIBS) -o $@

$(OBJDIR)/%.o: %.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	$(RM) -r $(OBJDIR) $(BIN)

$(TEST_BIN): $(TEST_OBJ) $(filter-out $(OBJDIR)/main.o,$(OBJ))
	$(CXX) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: $(BIN) $(TEST_BIN)
	$(PYTHON) tests/run_tests.py

-include $(DEPS)
