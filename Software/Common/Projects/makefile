#----------------------------------------------------------------------------
#
#  Name          makefile
#  Description : Makefile for the KeyTalk Common
#
# Usage:
#  make [release|debug|clean|install]
#  The default behavior is 'make debug'
#
#----------------------------------------------------------------------------

PRJS=libreseptcommon
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

install:


