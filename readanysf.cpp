#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>  //IMPORTANT bool
#include <stdlib.h>   // for malloc
#include <stdio.h>	//sprintf
#include <string.h>  //strcmp
#include <math.h>   // ceil

#include "m_pd.h"
#include "Readsf.h"

#define MAXSFCHANS 64	// got this from d_soundfile.c in pd/src


static t_class *readanysf_class;

typedef struct readanysf {
	t_object x_obj;
	t_sample *(x_outvec[MAXSFCHANS]);
	int frames ;   // this should be block size
	int num_channels;
	int buffersize;
	bool play;
	t_sample *buf;
	unsigned int count;
	t_outlet *outinfo;
	Readsf *rdsf;
} t_readanysf;


void m_play(t_readanysf *x) {
	if (x->rdsf->isReady() ) {	
		x->play = true;
	} else {
		post("Current file is still starting. ");
		post("This probably means that it is a stream and it needs to buffer in from the network.");
	}
}

void readanysf_bang(t_readanysf *x) {
	m_play(x);
}


void m_pause(t_readanysf *x) {
	x->play = false;
}

void m_pcm_seek(t_readanysf *x, float f) {
	if (! x->rdsf->PCM_seek( (long)f) )		
		post("can't seek on this file.");

}	

void m_stop(t_readanysf *x) {
	x->play = false;
	x->rdsf->Rewind();
}

void m_open_callback( void * data) {
	t_atom lst;
	t_readanysf * x = (t_readanysf *)data;
	
	SETFLOAT(&lst, (float)x->rdsf->get_input_samplerate() );
	outlet_anything(x->outinfo, gensym("samplerate"), 1, &lst);

	SETFLOAT(&lst, 1.0 );
	outlet_anything(x->outinfo, gensym("ready"), 1, &lst);

	SETFLOAT(&lst, x->rdsf->getLengthInSeconds() );
	outlet_anything(x->outinfo, gensym("length"), 1, &lst);
	
	// set time to 0 again here just to be sure
	outlet_float(x->outinfo, 0.0);

}

void m_open(t_readanysf *x, t_symbol *s) {

	t_atom lst;
	SETFLOAT(&lst, 0.0 );
	outlet_anything(x->outinfo, gensym("ready"), 1, &lst);

	SETFLOAT(&lst, 0.0 );
	outlet_anything(x->outinfo, gensym("length"), 1, &lst);
	
	outlet_float(x->outinfo, 0.0);

	x->play = false;
	x->rdsf->Open( s->s_name);
}

void m_speed(t_readanysf *x, float f) {
	x->rdsf->setSpeed(f);
}

void m_loop(t_readanysf *x, float f) {
	if ( f == 0)
		x->rdsf->doLoop( false );
	else 
		x->rdsf->doLoop( true );
	post("looping = %d", x->rdsf->doLoop());
}


static void *readanysf_new(t_float f ) {
  
  int nchannels = (int)f;
  int i;
  t_atom lst;

  // if the external is created without any options
  if (nchannels <=0)
	  nchannels = 2;

  t_readanysf *x = (t_readanysf *)pd_new(readanysf_class);
  x->num_channels = nchannels;
  x->count = 0;
  x->buffersize =0;
  x->rdsf = NULL; 
  x->play =false; 
  for (i=0; i < nchannels; i++) {
  	outlet_new(&x->x_obj,  gensym("signal"));
  }
  x->outinfo = outlet_new(&x->x_obj, &s_anything);
  SETFLOAT(&lst, 0.0 );
  outlet_anything(x->outinfo, gensym("ready"), 1, &lst);
  
  // set time to 0.0
  outlet_float(x->outinfo, 0.0);

  return (void *)x;
}

static t_int *readanysf_perform(t_int *w) {
	t_readanysf *x = (t_readanysf *) (w[1]);
	int i=0,j=0, k=0;
	int samples_returned = 0;
	int blocksize = x->frames; //sys_getblksize();
	t_atom lst;

	if (x->play ) {
		samples_returned = x->rdsf->Decode( (char *) x->buf, (unsigned int)blocksize);
		if (samples_returned == 0 && x->rdsf->getEOF() == true) {
			// if loop?
			m_stop(x);
			outlet_bang(x->outinfo);
		}
		for (i = 0; i < x->num_channels; i++) {
			k =0;
			for( j=0; j< samples_returned; j++) {
				x->x_outvec[i][j] = x->buf[ k + i  ];
				//if (j < 4)
				//	printf("%f,",  x->x_outvec[i][j]);
				k = k + x->num_channels;
			}
			//printf(" = %d | samps=%d\n", i, samples_returned);

		}
		//if (samples_returned != x->frames)
		//	printf("%d != %d\n", x->frames, samples_returned);
	} 

	for (i = 0; i < x->num_channels; i++) {
		for (j = samples_returned; j < x->frames;  j++) {
			x->x_outvec[i][j] = 0.0;
			
		}
	}
	if ( x->count++ > 1000 ) {
		SETFLOAT (&lst, x->rdsf->getFifoSizePercentage() );
		//post("cache: %f", x->rdsf->getFifoSizePercentage());
		outlet_anything(x->outinfo, gensym("cache"), 1, &lst);
		if (x->play) {
			outlet_float(x->outinfo, x->rdsf->getTimeInSeconds());
		}
		x->count = 0;
	}
	
	return (w+2);	
}

void readanysf_dsp(t_readanysf *x, t_signal **sp) {

  int i, tmpbufsize;

  x->frames = sp[0]->s_n;
  tmpbufsize = x->frames * x->num_channels  * sizeof(t_sample);
  //x->rdsf = new Readsf ( (int)sys_getsr(), nchannels, (sys_getblksize() + 1) * SRC_MAX  );
  if (x->rdsf == NULL)
  x->rdsf = new Readsf ( (int)sys_getsr(), x->num_channels, (x->frames + 1) * SRC_MAX );
  x->rdsf->setOpenCallback( m_open_callback, (void *)x); 
  // only malloc the buffer if the frame size changes.
  if(  x->buffersize < tmpbufsize ) { 
  	x->buffersize = tmpbufsize;
  	free(x->buf);
	x->buf = (t_sample *)malloc( x->buffersize  );
  	//post("frames=%d, buffersize = %d, spf=%d", x->frames, x->buffersize, x->samples_per_frame);
  }
  for (i = 0; i < x->num_channels; i++)
	  x->x_outvec[i] = sp[i]->s_vec;
		    
  dsp_add(readanysf_perform, 1, x);
}

static void readanysf_free(t_readanysf *x) {
	// delete the readany objs
	delete x->rdsf;
	x->rdsf = NULL;
}

//extern "C" void readanysf_tilde_setup(void) {
extern "C" void readanysf_tilde_setup(void) {

  readanysf_class = class_new(gensym("readanysf~"), (t_newmethod)readanysf_new,
  	(t_method)readanysf_free, sizeof(t_readanysf), 0, A_DEFFLOAT,  A_NULL);

  class_addmethod(readanysf_class, (t_method)readanysf_dsp, gensym("dsp"), A_NULL);
  class_addmethod(readanysf_class, (t_method)m_open, gensym("open"), A_SYMBOL, A_NULL);
  class_addmethod(readanysf_class, (t_method)m_play, gensym("play"),  A_NULL);
  class_addmethod(readanysf_class, (t_method)m_pause, gensym("pause"),  A_NULL);
  class_addmethod(readanysf_class, (t_method)m_stop, gensym("stop"),  A_NULL);
  class_addmethod(readanysf_class, (t_method)m_speed, gensym("speed"), A_FLOAT, A_NULL);
  class_addmethod(readanysf_class, (t_method)m_loop, gensym("loop"), A_FLOAT, A_NULL);
  class_addmethod(readanysf_class, (t_method)m_pcm_seek, gensym("pcm_seek"), A_FLOAT, A_NULL);
  class_addbang(readanysf_class, readanysf_bang);

}
