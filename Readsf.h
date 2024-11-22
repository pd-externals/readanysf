/*
 * Readsf object for reading and playing multiple soundfile types
 * from disk and from the web using gmerlin_avdecode
 *
 * Copyright (C) 2003 August Black
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Readsf.h
 */

#ifndef _READSF_H_
#define _READSF_H_

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "FifoAudioFrames.h"


#ifndef _AVDEC_H_
#define _AVDEC_H_
extern "C" {
#include <avdec.h>
}
#endif


#define STATE_EMPTY              0
#define STATE_STARTUP           1
#define STATE_READY            2
#define SRC_MAX                 256.0
#define SRC_MIN                 1/256.0

class Readsf  {

	public:
		Readsf( int sr, int nchannels, int fifosizeinsamples);
		~Readsf();
		void Open( char * filename);
		int Decode( char * buf, unsigned int lengthinsamples);
		bool Rewind();
		bool RewindNoFlush();
		bool PCM_seek(long bytes);
		bool TIME_seek(double seconds);
		int get_wanted_samplerate() { return output_audio_format.samplerate;}
		int get_input_samplerate() { return input_audio_format.samplerate;}
		int get_wanted_num_channels() { return output_audio_format.num_channels;}
		int get_input_num_channels() { return input_audio_format.num_channels;}
		int get_bytes_per_sample() { return bytes_per_sample;}
		float getLengthInSeconds(); 

		Fifo * getFifo() { return fifo; } 

		bgav_t * getFile() {return  file;}
		char * getFilename() {return  filename;}

		gavl_audio_frame_t * getIAF() { return iaf;}
		gavl_audio_frame_t * getOAF() { return oaf;}
		gavl_audio_frame_t * getTAF() { return taf;}
		gavl_audio_converter_t *  getT2OAudioConverter(){ return t2o_audio_converter;};
		gavl_audio_converter_t *  getI2TAudioConverter(){ return i2t_audio_converter;};
		bgav_options_t * getOpt() {return opt; }

		void setSpeed( float f);
		void setSRCFactor(float f) {  src_factor = f;}
		float getSRCFactor() { return src_factor;}

		void setState(int b) { this->state = b;}
		bool getState() { return state;}
		bool isReady() { if (state == STATE_READY) return true; else return false;}
	
		float getTimeInSeconds() { return timestamp / (float)input_audio_format.samplerate;};
		float getFifoSizePercentage() { return fifo->getSizePercentage();};

		void isOpen( bool b) { is_open = b; } 
		bool isOpen() { return is_open; } 

		void doLoop( bool b) { loop = b; } 
		bool doLoop() { return loop; } 


		void setEOF(bool b) { eof = b;}
		bool getEOF() { return eof;}

		bool doT2OConvert() { return do_t2o_convert; } 
		void doT2OConvert(bool b) { do_t2o_convert = b; } 

		bool doI2TConvert() { return do_i2t_convert; } 
		void doI2TConvert(bool b) { do_i2t_convert = b; } 



		int lockA() { return pthread_mutex_lock(&amut);}
		int unlockA() { return pthread_mutex_unlock(&amut);}


		void Wait() { pthread_cond_wait( &cond, &condmut); }
		void Signal() { pthread_cond_signal( &cond); }

		void setFile() { if (is_open) bgav_close(file);  file = bgav_create(); }
		void setOptions();
		void initFormat();
		void setOpenCallback(void (*oc)(void *), void *v ) { this->open_callback = oc; this->callback_data = v;  };
		void callOpenCallback() {	if(open_callback != NULL) this->open_callback( this->callback_data); };
		
		bool quit;

	private:
		void * callback_data;	
		void (* open_callback)(void * v);

		bool eof;
		int bytes_per_sample;
		int frames;
		int state;
		char filename[1024];
		float src_factor;
		bool do_t2o_convert;
		bool do_i2t_convert;
		bool is_open;
		bool loop;
		int samplesleft;
		int64_t timestamp;

		gavl_audio_options_t * aopt;
		bgav_t * file;
		bgav_options_t * opt;

		gavl_audio_converter_t *  i2t_audio_converter;
		gavl_audio_converter_t *  t2o_audio_converter;
		
		gavl_audio_frame_t * iaf;
		gavl_audio_frame_t * oaf;
		gavl_audio_frame_t * taf;

		gavl_audio_format_t input_audio_format;
		gavl_audio_format_t output_audio_format;
		gavl_audio_format_t tmp_audio_format;
		
		Fifo  *fifo;
	
		pthread_t thr_open;
		pthread_t thr_fillfifo;
		pthread_mutex_t condmut;
		pthread_mutex_t amut;
		pthread_cond_t cond;
		

};

#endif
