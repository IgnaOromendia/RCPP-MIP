ROOT := ..

CXX = g++
AR ?= ar
PKG_CONFIG ?= pkg-config

CPLEX_DIR ?= /Applications/CPLEX_Studio2211
CPLEX_PLATFORM ?= arm64_osx
CPLEX_INC = -isystem $(CPLEX_DIR)/cplex/include \
            -isystem $(CPLEX_DIR)/concert/include

GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0)

PROJECT_INCLUDES = -I$(ROOT)/cplexSolver -I$(ROOT)/cplexSolver/lib \
                   -I$(ROOT)/mipPathSort/lib \
                   -I$(ROOT)/mipSolver -I$(ROOT)/mipSolver/lib
CPPFLAGS += -DNDEBUG -DIL_STD $(GLIB_CFLAGS) $(CPLEX_INC) \
            $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions

OBJDIR ?= $(ROOT)/build
LIB ?= $(OBJDIR)/libpathsortcommon.a

COMMON_SRCS = $(ROOT)/mipPathSort/src/io/PathSortInstanceReader.cpp \
              $(ROOT)/mipPathSort/src/io/PathOrderWriter.cpp \
              $(ROOT)/mipPathSort/src/model/PathSolver.cpp \
              $(ROOT)/mipPathSort/src/constraints/OrderConstraintSetter.cpp \
              $(ROOT)/mipPathSort/src/constraints/PositionConstraintSetter.cpp
COMMON_OBJ = $(patsubst $(ROOT)/%.cpp,$(OBJDIR)/%.o,$(COMMON_SRCS))
DEPS = $(COMMON_OBJ:.o=.d)

.PHONY: all clean

all: $(LIB)

$(LIB): $(COMMON_OBJ) Makefile
	mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $(COMMON_OBJ)

$(OBJDIR)/%.o: $(ROOT)/%.cpp
	mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	$(RM) -r $(OBJDIR)/mipPathSort
	$(RM) $(LIB)

-include $(DEPS)
