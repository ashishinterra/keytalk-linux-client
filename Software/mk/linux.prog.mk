#############################################################################
#
#  Name          linux.prog.mk
#  Description : make include file for building executable, shared library or cxx unit test on Linux
#
#############################################################################

ifndef PROG
    $(error PROG variable should be defined)
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

LFLAGS=-ldl
ifeq ($(findstring debug,$(MAKECMDGOALS)),debug)
  LFLAGS+=-s
endif
ifdef EXTRA_LFLAGS
  LFLAGS+=$(EXTRA_LFLAGS)
endif

SRCS_CPP=$(filter %.cpp,$(SRCS))
SRCS_C=$(filter %.c,$(SRCS))
ifneq ($(SRCS_CPP), )
  COMPILER=$(CXX)
  LINKER=$(CXX)
else
  ifneq ($(SRCS_C), )
    COMPILER=$(CC)
    LINKER=$(CC)
  else
    $(error .c or .cpp extension is expected for source files)
  endif
endif

OBJS_CPP=$(SRCS_CPP:.cpp=.o)
OBJS_C=$(SRCS_C:.c=.o)
OBJS=$(OBJS_CPP) $(OBJS_C)
DEPS:=$(patsubst %,$(SRCS_DIR)/%,$(DEPS))

#############################################################################

.PHONY: release debug clean install $(DEP1) $(DEP2) $(DEP3) $(DEP4) $(DEP5) $(DEP6) $(DEP7) $(DEP8) $(DEP9) $(DEP10) $(DEP11) $(DEP12) $(DEP13) $(DEP14) $(DEP15) $(DEP16) $(DEP17) $(DEP18) $(DEP19) $(DEP20) $(DEP21) $(DEP22) $(DEP23) $(DEP24) $(DEP25) $(DEP26) $(DEP27) $(DEP28) $(DEP29) $(DEP30)

release debug: $(PRE_BUILD) $(OUT_DIR)/$(PROG) $(EXT_PROG1) $(EXT_PROG2) $(EXT_PROG3) $(POST_BUILD)

$(OUT_DIR)/$(PROG): $(DEP1) $(DEP2) $(DEP3) $(DEP4) $(DEP5) $(DEP6) $(DEP7) $(DEP8) $(DEP9) $(DEP10) $(DEP11) $(DEP12) $(DEP13) $(DEP14) $(DEP15) $(DEP16) $(DEP17) $(DEP18) $(DEP19) $(DEP20) $(DEP21) $(DEP22) $(DEP23) $(DEP24) $(DEP25) $(DEP26) $(DEP27) $(DEP28) $(DEP29) $(DEP30) $(OBJS)
	mkdir -p $(OUT_DIR)
	$(LINKER) $(OBJS) $(LIBS) $(LFLAGS) -o $@
ifdef LIB_SONAME
	cd $(OUT_DIR) && ln -sf $(PROG) $(LIB_SONAME) && cd $(CURDIR)
endif
ifdef LIB_LINKER_NAME
	cd $(OUT_DIR) && ln -sf $(PROG) $(LIB_LINKER_NAME) && cd $(CURDIR)
endif

$(DEP1):
	$(MAKE) -C $(DEP1_DIR) $(MAKECMDGOALS)

$(DEP2):
	$(MAKE) -C $(DEP2_DIR) $(MAKECMDGOALS)

$(DEP3):
	$(MAKE) -C $(DEP3_DIR) $(MAKECMDGOALS)

$(DEP4):
	$(MAKE) -C $(DEP4_DIR) $(MAKECMDGOALS)

$(DEP5):
	$(MAKE) -C $(DEP5_DIR) $(MAKECMDGOALS)

$(DEP6):
	$(MAKE) -C $(DEP6_DIR) $(MAKECMDGOALS)

$(DEP7):
	$(MAKE) -C $(DEP7_DIR) $(MAKECMDGOALS)

$(DEP8):
	$(MAKE) -C $(DEP8_DIR) $(MAKECMDGOALS)

$(DEP9):
	$(MAKE) -C $(DEP9_DIR) $(MAKECMDGOALS)

$(DEP10):
	$(MAKE) -C $(DEP10_DIR) $(MAKECMDGOALS)

$(DEP11):
	$(MAKE) -C $(DEP11_DIR) $(MAKECMDGOALS)

$(DEP12):
	$(MAKE) -C $(DEP12_DIR) $(MAKECMDGOALS)

$(DEP13):
	$(MAKE) -C $(DEP13_DIR) $(MAKECMDGOALS)

$(DEP14):
	$(MAKE) -C $(DEP14_DIR) $(MAKECMDGOALS)

$(DEP15):
	$(MAKE) -C $(DEP15_DIR) $(MAKECMDGOALS)

$(DEP16):
	$(MAKE) -C $(DEP16_DIR) $(MAKECMDGOALS)

$(DEP17):
	$(MAKE) -C $(DEP17_DIR) $(MAKECMDGOALS)

$(DEP18):
	$(MAKE) -C $(DEP18_DIR) $(MAKECMDGOALS)

$(DEP19):
	$(MAKE) -C $(DEP19_DIR) $(MAKECMDGOALS)

$(DEP20):
	$(MAKE) -C $(DEP20_DIR) $(MAKECMDGOALS)

$(DEP21):
	$(MAKE) -C $(DEP21_DIR) $(MAKECMDGOALS)

$(DEP22):
	$(MAKE) -C $(DEP22_DIR) $(MAKECMDGOALS)

$(DEP23):
	$(MAKE) -C $(DEP23_DIR) $(MAKECMDGOALS)

$(DEP24):
	$(MAKE) -C $(DEP24_DIR) $(MAKECMDGOALS)

$(DEP25):
	$(MAKE) -C $(DEP25_DIR) $(MAKECMDGOALS)

$(DEP26):
	$(MAKE) -C $(DEP26_DIR) $(MAKECMDGOALS)

$(DEP27):
	$(MAKE) -C $(DEP27_DIR) $(MAKECMDGOALS)

$(DEP28):
	$(MAKE) -C $(DEP28_DIR) $(MAKECMDGOALS)

$(DEP29):
	$(MAKE) -C $(DEP29_DIR) $(MAKECMDGOALS)

$(DEP30):
	$(MAKE) -C $(DEP30_DIR) $(MAKECMDGOALS)


%.o:: $(SRCS_DIR)/%.c $(DEPS)
	$(COMPILER) $(INCLUDES) $(CFLAGS) $< -o $@

%.o:: $(SRCS_DIR)/%.cpp $(DEPS)
	$(COMPILER) $(INCLUDES) $(CFLAGS) $< -o $@

ifdef CXX_TESTS
  ifndef CXX_TEST_GENERATOR
    $(error CXX_TEST_GENERATOR variable should be set when CXX_TESTS is set)
  endif
  ifndef CXX_TEST_GENERATED_CPP
    $(error CXX_TEST_GENERATED_CPP variable should be set when CXX_TESTS is set)
  endif
$(SRCS_DIR)/$(CXX_TEST_GENERATED_CPP): $(CXX_TESTS)
	$(CXX_TEST_GENERATOR) --have-eh --have-std --part -o $@ $^
endif

#
# CLEAN
#

clean: $(EXT_PROG1_CLEAN) $(EXT_PROG2_CLEAN) $(EXT_PROG3_CLEAN) _CLEAN_ $(POST_CLEAN)

_CLEAN_:
	-rm -f $(OUT_DIR)/$(PROG) $(OBJS)
ifdef CXX_TESTS
ifdef CXX_TEST_GENERATED_CPP
	-rm -f $(SRCS_DIR)/$(CXX_TEST_GENERATED_CPP)
endif
endif

ifdef LIB_SONAME
	-rm -f $(OUT_DIR)/$(LIB_SONAME)
endif
ifdef LIB_LINKER_NAME
	-rm -f $(OUT_DIR)/$(LIB_LINKER_NAME)
endif
ifdef DEP1_DIR
	$(MAKE) -C $(DEP1_DIR) clean
endif
ifdef DEP2_DIR
	$(MAKE) -C $(DEP2_DIR) clean
endif
ifdef DEP3_DIR
	$(MAKE) -C $(DEP3_DIR) clean
endif
ifdef DEP4_DIR
	$(MAKE) -C $(DEP4_DIR) clean
endif
ifdef DEP5_DIR
	$(MAKE) -C $(DEP5_DIR) clean
endif
ifdef DEP6_DIR
	$(MAKE) -C $(DEP6_DIR) clean
endif
ifdef DEP7_DIR
	$(MAKE) -C $(DEP7_DIR) clean
endif
ifdef DEP8_DIR
	$(MAKE) -C $(DEP8_DIR) clean
endif
ifdef DEP9_DIR
	$(MAKE) -C $(DEP9_DIR) clean
endif
ifdef DEP10_DIR
	$(MAKE) -C $(DEP10_DIR) clean
endif
ifdef DEP11_DIR
	$(MAKE) -C $(DEP11_DIR) clean
endif
ifdef DEP12_DIR
	$(MAKE) -C $(DEP12_DIR) clean
endif
ifdef DEP13_DIR
	$(MAKE) -C $(DEP13_DIR) clean
endif
ifdef DEP14_DIR
	$(MAKE) -C $(DEP14_DIR) clean
endif
ifdef DEP15_DIR
	$(MAKE) -C $(DEP15_DIR) clean
endif
ifdef DEP16_DIR
	$(MAKE) -C $(DEP16_DIR) clean
endif
ifdef DEP17_DIR
	$(MAKE) -C $(DEP17_DIR) clean
endif
ifdef DEP18_DIR
	$(MAKE) -C $(DEP18_DIR) clean
endif
ifdef DEP19_DIR
	$(MAKE) -C $(DEP19_DIR) clean
endif
ifdef DEP20_DIR
	$(MAKE) -C $(DEP20_DIR) clean
endif
ifdef DEP21_DIR
	$(MAKE) -C $(DEP21_DIR) clean
endif
ifdef DEP22_DIR
	$(MAKE) -C $(DEP22_DIR) clean
endif
ifdef DEP23_DIR
	$(MAKE) -C $(DEP23_DIR) clean
endif
ifdef DEP24_DIR
	$(MAKE) -C $(DEP24_DIR) clean
endif
ifdef DEP25_DIR
	$(MAKE) -C $(DEP25_DIR) clean
endif
ifdef DEP26_DIR
	$(MAKE) -C $(DEP26_DIR) clean
endif
ifdef DEP27_DIR
	$(MAKE) -C $(DEP27_DIR) clean
endif
ifdef DEP28_DIR
	$(MAKE) -C $(DEP28_DIR) clean
endif
ifdef DEP29_DIR
	$(MAKE) -C $(DEP29_DIR) clean
endif
ifdef DEP30_DIR
	$(MAKE) -C $(DEP30_DIR) clean
endif
