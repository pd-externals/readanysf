#ifndef _FIFOAUDIOFRAMES_H_
#define _FIFOAUDIOFRAMES_H_

#include <string.h> // memcpy
#include <stdio.h>
#include <pthread.h>

#ifndef _AVDEC_H_
#define _AVDEC_H_
extern "C" {
#include <avdec.h>
}
#endif

class Fifo {
	public:
		Fifo(int s, const gavl_audio_format_t * format) ; 
		~Fifo();// { delete [] fifoPtr; }
		int Append(const gavl_audio_frame_t * af);
		int Get( gavl_audio_frame_t * af) ;  // pop an element off the fifo
		void Flush();
		int FreeSpace() { return size - count;}
		int isEmpty() { pthread_mutex_lock(&mut); bool c = (count == 0) ; pthread_mutex_unlock (&mut); return c;}
		int isFull()  {  pthread_mutex_lock(&mut); bool c = (count == size ); pthread_mutex_unlock (&mut); return c; } 
		gavl_audio_format_t * getFormat() { return format; };
		float getSizePercentage() { return count / (float) size; };
	private:
		int size ;  // Number of elements on Fifo
		int start ;
		int end ;
		int count;
		gavl_audio_frame_t ** fifoPtr ;  
		gavl_audio_format_t * format;  
		pthread_mutex_t mut;
} ;


#endif

