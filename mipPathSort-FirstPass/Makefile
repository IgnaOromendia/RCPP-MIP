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
                   -I$(ROOT)/mipPathSort-Common/lib \
                   -I$(ROOT)/mipPathSort-FirstPass/lib \
                   -I$(ROOT)/tests
CPPFLAGS += -DNDEBUG -DIL_STD $(GLIB_CFLAGS) $(CPLEX_INC) \
            $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
LDLIBS += $(GLIB_LIBS) -lm -lpthread

OBJDIR ?= $(ROOT)/build
LIB ?= $(OBJDIR)/libpathsorterfirstpass.a
COMMON_LIB ?= $(OBJDIR)/libpathsortcommon.a
COMMON_INPUTS = $(wildcard $(ROOT)/mipPathSort-Common/lib/io/*.h \
                $(ROOT)/mipPathSort-Common/lib/model/*.h \
                $(ROOT)/mipPathSort-Common/src/io/*.cpp) \
                $(ROOT)/mipPathSort-Common/Makefile

FIRST_PASS_SRCS = $(ROOT)/mipPathSort-FirstPass/src/model/PathSorterFirstPass.cpp \
                  $(ROOT)/mipPathSort-FirstPass/src/constraints/FirstPassConstraintSetter.cpp \
                  $(ROOT)/mipPathSort-FirstPass/src/constraints/ModuleConstraintSetter.cpp \
                  $(ROOT)/mipPathSort-FirstPass/src/constraints/OrderConstraintSetter.cpp \
                  $(ROOT)/mipPathSort-FirstPass/src/constraints/PositionConstraintSetter.cpp
FIRST_PASS_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(FIRST_PASS_SRCS))

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
MAIN_OBJ = $(OBJDIR)/mipPathSort-FirstPass/src/main.o
BIN ?= $(ROOT)/pathSortExec

DEPS = $(FIRST_PASS_OBJ:.o=.d) $(SUPPORT_OBJ:.o=.d) $(MIP_CORE_OBJ:.o=.d) \
       $(MAIN_OBJ:.o=.d) $(PATH_TEST_OBJ:.o=.d) $(READER_TEST_OBJ:.o=.d) \
       $(LIFETIME_TEST_OBJ:.o=.d) $(WRITER_TEST_OBJ:.o=.d)

.PHONY: all clean test-binaries

all: $(COMMON_LIB) $(LIB) $(BIN)

$(COMMON_LIB): $(COMMON_INPUTS)
	$(MAKE) -C $(ROOT)/mipPathSort-Common OBJDIR=$(abspath $(OBJDIR)) \
		LIB=$(abspath $(COMMON_LIB)) all

$(LIB): $(FIRST_PASS_OBJ) Makefile
	mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $(FIRST_PASS_OBJ)

$(BIN): $(MAIN_OBJ) $(FIRST_PASS_OBJ) $(COMMON_LIB) $(SUPPORT_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(OBJDIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(PATH_TEST_BIN): $(PATH_TEST_OBJ) $(FIRST_PASS_OBJ) $(COMMON_LIB) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(READER_TEST_BIN): $(READER_TEST_OBJ) $(FIRST_PASS_OBJ) $(COMMON_LIB) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(LIFETIME_TEST_BIN): $(LIFETIME_TEST_OBJ) $(FIRST_PASS_OBJ) $(COMMON_LIB) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

$(WRITER_TEST_BIN): $(WRITER_TEST_OBJ) $(FIRST_PASS_OBJ) $(COMMON_LIB) $(MIP_CORE_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

test-binaries: $(PATH_TEST_BIN) $(READER_TEST_BIN) $(LIFETIME_TEST_BIN) $(WRITER_TEST_BIN)

clean:
	$(RM) -r $(OBJDIR)/mipPathSort-FirstPass
	$(RM) $(LIB) $(PATH_TEST_BIN) $(READER_TEST_BIN) $(LIFETIME_TEST_BIN) \
	      $(WRITER_TEST_BIN) $(PATH_TEST_OBJ) $(READER_TEST_OBJ) $(LIFETIME_TEST_OBJ) \
	      $(WRITER_TEST_OBJ) $(MAIN_OBJ) $(BIN)

-include $(DEPS)
