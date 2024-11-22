#include "FifoAudioFrames.h"


Fifo::Fifo(int s, const gavl_audio_format_t * f) {
	size = s  ;  
	start = 0 ;
	end = 0 ; 
	count = 0;
	format = new gavl_audio_format_t;
	gavl_audio_format_copy(format, f);
	fifoPtr = new gavl_audio_frame_t * [size] ;
	for(int i=0; i < size; i++) {
		fifoPtr[i] = gavl_audio_frame_create( format );
	}
	
	pthread_mutex_init (&mut, 0);
	//printf("size=%d\n", size);
}
Fifo::~Fifo() {
	printf("deleting fifo \n");
	for(int i=0; i < size; i++) {
		 gavl_audio_frame_destroy( fifoPtr[i] );
	}
	printf("deleted fifo \n");
}
// empty the fifo of any content
void Fifo::Flush() {
	pthread_mutex_lock (&mut);
	start = 0 ; 
	end = 0 ; 
	count = 0;
	//printf("flushed size=%d\n", count);
	pthread_mutex_unlock (&mut);
}

// push an element onto the Fifo 
int Fifo::Append( const gavl_audio_frame_t * source) {
	pthread_mutex_lock (&mut);
	//printf("adding one to fifoPtr[%d], start=%d, end=%d, count %d, size=%d\n", end , start, end, count, size);
	if ( count < size ) {  // if there is room for one more
		if (end >= size)
			end = 0;
		gavl_audio_frame_copy(format, fifoPtr[end],  source, 0,0, format->samples_per_frame, format->samples_per_frame) ;
		fifoPtr[end]->timestamp = source->timestamp;
		end++;	
		count++; //increment count
		//printf("added one to fifoPtr[%d], start=%d, end=%d, count %d, size=%d\n", end -1, start, end, count, size);
		pthread_mutex_unlock (&mut);
		return 1 ;  // push successful
	} // no room in fifo, return false
	pthread_mutex_unlock (&mut);
	return 0 ;  // push unsuccessful
}

// get an element off the Fifo
int Fifo::Get( gavl_audio_frame_t * dest) {
	pthread_mutex_lock (&mut);
	if ( count > 0) {  // if there any items in the fifo
		if ( start >= size)
			start = 0;
		gavl_audio_frame_copy(format, dest, fifoPtr[start], 0, 0, format->samples_per_frame, format->samples_per_frame) ;
		dest->timestamp = fifoPtr[start]->timestamp;
		start++;
		count--;
		//printf("got one from fifoPtr[%d], start=%d, end=%d, size=%d\n", start -1, start,end,count);
		pthread_mutex_unlock (&mut);
		return 1 ;  // pop successful
	}
	pthread_mutex_unlock (&mut);
	return 0 ;  // pop unsuccessful
}



