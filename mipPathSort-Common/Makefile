ROOT := ..

CXX = g++
AR ?= ar
PKG_CONFIG ?= pkg-config

GLIB_CFLAGS = $(shell $(PKG_CONFIG) --cflags glib-2.0)

PROJECT_INCLUDES = -I$(ROOT)/mipPathSort-Common/lib \
                   -I$(ROOT)/mipSolver -I$(ROOT)/mipSolver/lib
CPPFLAGS += -DNDEBUG $(GLIB_CFLAGS) $(PROJECT_INCLUDES)
CXXFLAGS ?= -O3 -std=c++17 -fPIC -fexceptions

OBJDIR ?= $(ROOT)/build
LIB ?= $(OBJDIR)/libpathsortcommon.a

COMMON_SRCS = $(ROOT)/mipPathSort-Common/src/io/PathSortInstanceReader.cpp \
              $(ROOT)/mipPathSort-Common/src/io/PathOrderWriter.cpp
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
	$(RM) -r $(OBJDIR)/mipPathSort-Common
	$(RM) $(LIB)

-include $(DEPS)
