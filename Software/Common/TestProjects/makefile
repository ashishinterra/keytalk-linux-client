#----------------------------------------------------------------------------
#
#  Name          makefile
#  Description : Makefile for the KeyTalk Common tests
#
# Usage:
#   make [release|debug|clean]
#
#----------------------------------------------------------------------------

PRJS=testlibreseptcommon
INST_PRJS=$(PRJS)
MAKE_FLAGS=-s

all: release

release debug:
	@set -e;			\
	for dir in $(PRJS); do		\
		echo "-- Building $$dir for $@";	\
		make -C $$dir ${MAKE_FLAGS} $@;		\
	done;

clean:
	@set -e;			\
	for dir in $(PRJS); do		\
		echo "-- Cleaning $$dir";	\
		make -C $$dir ${MAKE_FLAGS} clean;		\
	done;
	@rm -rf ./Export/
