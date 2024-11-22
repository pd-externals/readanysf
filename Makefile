# Makefile for readanysf~

lib.name = readanysf~

class.sources = \
        src/readanysf~.cpp

shared.sources = \
        src/FifoAudioFrames.cpp \
        src/FifoVideoFrames.cpp \
        src/ReadMedia.cpp

ldlibs = -lgmerlin_avdec -lgavl

datafiles = \
        readanysf~-help.pd \
        readanysf~-meta.pd \
        README \
        LICENSE.md

# This Makefile is based on the Makefile from pd-lib-builder written by
# Katja Vetter. You can get it from:
# https://github.com/pure-data/pd-lib-builder

PDLIBBUILDER_DIR=pd-lib-builder/
include $(firstword $(wildcard $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder Makefile.pdlibbuilder))


localdep_linux: install
	scripts/localdeps.linux.sh -d "${installpath}/readanysf~.${extension}"

localdep_macos: install
	scripts/localdeps.macos.sh -d -s "${installpath}/readanysf~.${extension}"

localdep_windows: install
	scripts/localdeps.win.sh "${installpath}/readanysf~.${extension}"
