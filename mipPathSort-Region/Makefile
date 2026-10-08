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
                   -I$(ROOT)/mipPathSort-Common/lib \
                   -I$(ROOT)/mipPathSort-Region/lib
CPPFLAGS += -DNDEBUG -DIL_STD $(GLIB_CFLAGS) $(CPLEX_INC) \
            $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions
LDLIBS += $(GLIB_LIBS) -lm -lpthread

OBJDIR ?= $(ROOT)/build
LIB ?= $(OBJDIR)/libpathsorterregion.a
COMMON_LIB ?= $(OBJDIR)/libpathsortcommon.a
COMMON_INPUTS = $(wildcard $(ROOT)/mipPathSort-Common/lib/io/*.h \
                $(ROOT)/mipPathSort-Common/lib/model/*.h \
                $(ROOT)/mipPathSort-Common/src/io/*.cpp) \
                $(ROOT)/mipPathSort-Common/Makefile

REGION_SRCS = $(ROOT)/mipPathSort-Region/src/model/PathSorterRegion.cpp
REGION_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(REGION_SRCS))
CPLEX_OBJ = $(OBJDIR)/cplexSolver/src/CPLEXSolver.o
REGION_TEST_OBJ = $(OBJDIR)/tests/path_sorter_region_test.o
REGION_TEST_BIN = $(OBJDIR)/path_sorter_region_test
DEPS = $(REGION_OBJ:.o=.d) $(CPLEX_OBJ:.o=.d) $(REGION_TEST_OBJ:.o=.d)

.PHONY: all clean test-binaries

all: $(COMMON_LIB) $(LIB)

$(COMMON_LIB): $(COMMON_INPUTS)
	$(MAKE) -C $(ROOT)/mipPathSort-Common OBJDIR=$(abspath $(OBJDIR)) \
		LIB=$(abspath $(COMMON_LIB)) all

$(LIB): $(REGION_OBJ) Makefile
	mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $(REGION_OBJ)

$(OBJDIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(REGION_TEST_BIN): $(REGION_TEST_OBJ) $(REGION_OBJ) $(CPLEX_OBJ)
	$(CXX) $(LDFLAGS) $(CPLEX_LIB) $^ $(CPLEX_LDLIBS) $(LDLIBS) -o $@

test-binaries: $(REGION_TEST_BIN)

clean:
	$(RM) -r $(OBJDIR)/mipPathSort-Region
	$(RM) $(LIB) $(REGION_TEST_OBJ) $(REGION_TEST_OBJ:.o=.d) $(REGION_TEST_BIN)

-include $(DEPS)
