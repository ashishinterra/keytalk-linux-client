#############################################################################
#
#  Name          linux.staticlib.mk
#  Description : make include file for building static library on Linux
#
#############################################################################

ifndef STATIC_LIB
    $(error STATIC_LIB variable should be defined)
endif

CXX=clang++
CC=clang

CFLAGS=-c -std=c++03 -I/usr/local/include -Wall -Wextra -Wunused -Wno-missing-field-initializers -Wpointer-arith -Wcast-align -Wstrict-overflow=5 -Wwrite-strings -Wcast-qual -pedantic -Wno-long-long -Wformat=2 -Winit-self -Wmissing-include-dirs -Wcast-align -Wvariadic-macros -Woverlength-strings -Wctor-dtor-privacy -Wreorder -Woverloaded-virtual
ifeq ($(findstring debug,$(MAKECMDGOALS)),debug)
  CFLAGS+=-g -DDEBUG
else
  CFLAGS+=-DNDEBUG
endif
ifdef EXTRA_CFLAGS
  CFLAGS+=$(EXTRA_CFLAGS)
endif

SRCS_CPP=$(filter %.cpp,$(SRCS))
SRCS_C=$(filter %.c,$(SRCS))
ifneq ($(SRCS_CPP), )
  COMPILER=$(CXX)
else ifneq ($(SRCS_C), )
  COMPILER=$(CC)
else
  $(error .c or .cpp extension is expected for source files)
endif

OBJS_CPP=$(SRCS_CPP:.cpp=.o)
OBJS_C=$(SRCS_C:.c=.o)
OBJS=$(OBJS_CPP) $(OBJS_C)
DEPS:=$(patsubst %,$(SRCS_DIR)/%,$(DEPS))

#############################################################################

.PHONY: release debug clean install

release debug: $(PRE_BUILD) $(OUT_DIR)/$(STATIC_LIB) $(POST_BUILD)

$(OUT_DIR)/$(STATIC_LIB): $(OBJS)
	mkdir -p $(OUT_DIR)
	ar rcs $@ $(OBJS)

%.o:: $(SRCS_DIR)/%.c $(DEPS)
	$(COMPILER) $(INCLUDES) $(CFLAGS) $< -o $@

%.o:: $(SRCS_DIR)/%.cpp $(DEPS)
	$(COMPILER) $(INCLUDES) $(CFLAGS) $< -o $@

#
# CLEAN
#

clean: _CLEAN_ $(POST_CLEAN)

_CLEAN_:
	-rm -f $(OBJS) $(OUT_DIR)/$(STATIC_LIB)

