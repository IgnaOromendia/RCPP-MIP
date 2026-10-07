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
                   -I$(ROOT)/mipPathSort -I$(ROOT)/mipPathSort/lib \
                   -I$(ROOT)/tests
CPPFLAGS += -DNDEBUG -DIL_STD $(GLIB_CFLAGS) $(CPLEX_INC) \
            $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
LDLIBS += $(GLIB_LIBS) -lm -lpthread

OBJDIR ?= $(ROOT)/build
LIB ?= $(OBJDIR)/libpathsorter.a

PATH_SRCS = $(ROOT)/mipPathSort/src/model/PathSorter.cpp \
            $(ROOT)/mipPathSort/src/io/PathSortInstanceReader.cpp \
            $(ROOT)/mipPathSort/src/io/PathOrderWriter.cpp \
            $(ROOT)/mipPathSort/src/constraints/FirstPassConstraintSetter.cpp \
            $(ROOT)/mipPathSort/src/constraints/ModuleConstraintSetter.cpp \
            $(ROOT)/mipPathSort/src/constraints/OrderConstraintSetter.cpp \
            $(ROOT)/mipPathSort/src/constraints/PositionConstraintSetter.cpp
PATH_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(PATH_SRCS))

SUPPORT_SRCS = $(ROOT)/cplexSolver/src/CPLEXSolver.cpp \
               $(ROOT)/mipSolver/src/graph/Graph.cpp \
               $(ROOT)/mipSolver/src/graph/SuperGraph.cpp \
               $(ROOT)/mipSolver/src/io/InstanceReader.cpp \
               $(ROOT)/mipSolver/src/model/Instance.cpp \
               $(ROOT)/mipSolver/src/util/HashMap.cpp
SUPPORT_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(SUPPORT_SRCS))

MIP_CORE_SRCS = $(wildcard $(ROOT)/cplexSolver/src/*.cpp \
                $(ROOT)/mipSolver/src/constraints/*.cpp \
                $(ROOT)/mipSolver/src/io/*.cpp \
                $(ROOT)/mipSolver/src/graph/*.cpp \
                $(ROOT)/mipSolver/src/model/*.cpp \
                $(ROOT)/mipSolver/src/heuristic/*.cpp \
                $(ROOT)/mipSolver/src/util/*.cpp)
MIP_CORE_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(MIP_CORE_SRCS))

PATH_TEST_OBJ = $(OBJDIR)/tests/path_sorter_test.o
PATH_TEST_BIN = $(OBJDIR)/path_sorter_test
READER_TEST_OBJ = $(OBJDIR)/tests/path_sort_instance_reader_test.o
READER_TEST_BIN = $(OBJDIR)/path_sort_instance_reader_test
LIFETIME_TEST_OBJ = $(OBJDIR)/tests/solver_lifetime_test.o
LIFETIME_TEST_BIN = $(OBJDIR)/solver_lifetime_test
WRITER_TEST_OBJ = $(OBJDIR)/tests/path_order_writer_test.o
WRITER_TEST_BIN = $(OBJDIR)/path_order_writer_test
MAIN_OBJ = $(OBJDIR)/mipPathSort/src/main.o
BIN ?= $(ROOT)/pathSortExec

DEPS = $(PATH_OBJ:.o=.d) $(SUPPORT_OBJ:.o=.d) $(MIP_CORE_OBJ:.o=.d) $(MAIN_OBJ:.o=.d) \
       $(PATH_TEST_OBJ:.o=.d) $(READER_TEST_OBJ:.o=.d) $(LIFETIME_TEST_OBJ:.o=.d)
DEPS += $(WRITER_TEST_OBJ:.o=.d)

.PHONY: all clean test-binaries

all: $(LIB) $(BIN)

$(LIB): $(PATH_OBJ) $(SUPPORT_OBJ) Makefile
	mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $(PATH_OBJ) $(SUPPORT_OBJ)

$(BIN): $(MAIN_OBJ) $(PATH_OBJ) $(SUPPORT_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(OBJDIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(PATH_TEST_BIN): $(PATH_TEST_OBJ) $(PATH_OBJ) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(READER_TEST_BIN): $(READER_TEST_OBJ) $(PATH_OBJ) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(LIFETIME_TEST_BIN): $(LIFETIME_TEST_OBJ) $(PATH_OBJ) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(WRITER_TEST_BIN): $(WRITER_TEST_OBJ) $(PATH_OBJ) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

test-binaries: $(PATH_TEST_BIN) $(READER_TEST_BIN) $(LIFETIME_TEST_BIN) $(WRITER_TEST_BIN)

clean:
	$(RM) -r $(OBJDIR)/mipPathSort
	$(RM) $(LIB) $(PATH_TEST_BIN) $(READER_TEST_BIN) $(LIFETIME_TEST_BIN) \
	      $(WRITER_TEST_BIN) $(PATH_TEST_OBJ) $(READER_TEST_OBJ) $(LIFETIME_TEST_OBJ) \
	      $(WRITER_TEST_OBJ) $(MAIN_OBJ) $(BIN)

-include $(DEPS)
