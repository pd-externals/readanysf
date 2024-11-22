RTAFLAGS= -D__LINUX_ALSA__ -I./rtaudio-4.0.4/include/ -I ./rtaudio-4.0.4/  
CFLAGS =  -I./  -I/usr/local/include/gavl -I/usr/local/include/gmerlin -I/usr/local/include -Wall
LDFLAGS =  -L/usr/local/lib -lpthread  -lgavl -lgmerlin_avdec 

LINUXCFLAGS = -O1 -funroll-loops -fomit-frame-pointer \
    -Wall -W -Wshadow -Wstrict-prototypes \
    -Wno-unused -Wno-parentheses -Wno-switch



MACCFLAGS = -O2 -Wno-deprecated -I/usr/local/include/SDL  `/usr/local/bin/sdl-config --cflags` 
MACLDFLAGS =  -lsndfile  -framework AudioToolbox -framework Foundation -framework AppKit `/usr/local/bin/sdl-config --libs`



all:  pd 

playaudio: objs/main.o objs/Readsf.o objs/RtAudio.o objs/FifoAudioFrames.o Readsf.cpp Readsf.h FifoAudioFrames.h main.cpp 
	g++  -o  playaudio  \
	objs/main.o \
	objs/RtAudio.o \
	objs/FifoAudioFrames.o \
	objs/Readsf.o  $(LDFLAGS) -lasound


pd: readanysf.cpp Readsf.cpp Readsf.h  objs/FifoAudioFrames.o objs/Readsf.o FifoAudioFrames.h FifoAudioFrames.cpp
	g++  -shared  -o  readanysf~.pd_linux  $(CFLAGS) $(LDFLAGS) \
	readanysf.cpp \
	objs/FifoAudioFrames.o \
	objs/Readsf.o 
	strip --strip-unneeded readanysf~.pd_linux

objs/Readsf.o: Readsf.cpp Readsf.h FifoAudioFrames.h 
	g++  -c -o objs/Readsf.o Readsf.cpp $(CFLAGS)

objs/FifoAudioFrames.o: FifoAudioFrames.cpp FifoAudioFrames.h 
	g++  -c -o objs/FifoAudioFrames.o FifoAudioFrames.cpp $(CFLAGS)

clean:
	rm objs/*.o
