#include "Readsf.h"
#include <pthread.h>

#define SPF  64 

static void *thread_open(void * xp);
static void *fill_fifo(void * xp);

Readsf::Readsf( int sr, int nchannels, int fifosizeinsamples ) {

	state = STATE_EMPTY;
	src_factor = 1.0;

	opt = NULL;
	aopt = gavl_audio_options_create ();
	gavl_audio_options_set_resample_mode (aopt, GAVL_RESAMPLE_SINC_MEDIUM);
	gavl_audio_options_set_quality( aopt, 3 );  //1-5, 5 is the best

	t2o_audio_converter = gavl_audio_converter_create( );
	i2t_audio_converter = gavl_audio_converter_create( );

	oaf = NULL;
	iaf = NULL;
	taf = NULL;
	
	is_open = false;
	do_t2o_convert = false;
	do_i2t_convert = false;
	
	// assume we are always using the GAVL_SAMPLE_FLOAT format
	bytes_per_sample = sizeof(float);
	samplesleft = 0;
	timestamp = 0;

	// set up our intermediary format so that it has
	// floating point buffers, interlaced, with proper number
	// of channels.  this will be fixed.
	tmp_audio_format.samples_per_frame = SPF; 
	tmp_audio_format.sample_format = GAVL_SAMPLE_FLOAT ;
	tmp_audio_format.interleave_mode = GAVL_INTERLEAVE_ALL  ;
	tmp_audio_format.num_channels = nchannels;
	tmp_audio_format.channel_locations[0] = GAVL_CHID_NONE; // Reset
	tmp_audio_format.samplerate = sr;   // we need to really set this to whatever the file in is
	gavl_set_channel_setup (&tmp_audio_format); // Set channel locations

	output_audio_format.sample_format = GAVL_SAMPLE_FLOAT ;
	//output_audio_format.interleave_mode = GAVL_INTERLEAVE_NONE ;
	output_audio_format.interleave_mode = GAVL_INTERLEAVE_ALL ;
	output_audio_format.num_channels = nchannels;
	output_audio_format.channel_locations[0] = GAVL_CHID_NONE; // Reset
	output_audio_format.samplerate = sr;

	//post("output format sr: %d, rdsf->frames %d", rdsf->output_audio_format.samplerate, rdsf->frames);
	output_audio_format.samples_per_frame = SPF * SRC_MAX +10;
	gavl_set_channel_setup (&output_audio_format); // Set channel locations

	taf = gavl_audio_frame_create(&tmp_audio_format);
	oaf = gavl_audio_frame_create(&output_audio_format);
	
	fifo= new Fifo( 24,  &tmp_audio_format); 
	
	open_callback = NULL;
	callback_data = NULL;

	quit = false;
	loop = false;
	
	sprintf(filename, "what up foool!");

	pthread_cond_init(&cond, 0);
	pthread_mutex_init(&condmut, 0);
	pthread_mutex_init(&amut, 0);
	pthread_create(&thr_fillfifo, NULL, fill_fifo, (void *)this);
}

Readsf::~Readsf() {
	quit = true;
	Signal();
	Signal();
	printf("destroying Readsf\n");
	pthread_join( thr_fillfifo, NULL);
	printf("joined fillfifo thread\n");
	pthread_join( thr_open, NULL);
	printf("joined the open thread\n");
	if (oaf != NULL) {
		gavl_audio_frame_destroy(oaf);
		//printf("destroyed oaf\n");
	}
	if (iaf != NULL) {
		gavl_audio_frame_destroy(iaf);
		//printf("destroyed iaf\n");
	}
	if (taf != NULL) {
		gavl_audio_frame_destroy(taf);
		//printf("destroyed taf\n");
	}
	gavl_audio_converter_destroy(t2o_audio_converter);
	gavl_audio_converter_destroy(i2t_audio_converter);
	if (is_open) {
		bgav_close(file);
		//printf("closed file\n");
	}
	//printf("now, on to deleting fifo...\n");
	delete fifo;
}

int Readsf::Decode( char * buf, unsigned int lengthinsamples ) {
	int lis = lengthinsamples;
	unsigned int ret,writesize,totalwritesize = 0;
	int bufcnt=0;
	int num_channels = output_audio_format.num_channels;

	while (lis > 0) {
		if ( lis <= samplesleft) {
			writesize = lis * num_channels * bytes_per_sample;
			if( oaf->valid_samples < samplesleft)
				printf("Readsf::Decode,  valid_samples < samplesleft, shouldn't happen\n");
			//printf("OUCH!! lis <= samplesleft| oaf->vs %d, lis=%d, samplesleft=%d, writesize=%d, bufcnt=%d\n", 
			//oaf->valid_samples, lis, samplesleft, writesize, bufcnt);
			// we should watch here.  if oaf->valid_samples is less than samplesleft, we have problems!
			memcpy(  (buf+bufcnt), (void *)( oaf->samples.f + ((oaf->valid_samples  - samplesleft ) * num_channels) ), writesize );
			samplesleft -= lis; 
			lis = 0;
			totalwritesize += writesize;
			bufcnt += writesize;
			//printf("lis <= samplesleft oaf->vs %d, lis=%d  samplesleft=%d, bufcnt=%d\n",
			//oaf->valid_samples, lis, samplesleft, bufcnt);
			break;
		} else if (samplesleft > 0) {
			writesize = samplesleft * num_channels * bytes_per_sample;
			if( oaf->valid_samples < samplesleft)
				printf("Readsf::Decode,  valid_samples < samplesleft, shouldn't happen\n");
			//	printf("samplesleft > 0 oaf->vs %d, lis=%d, samplesleft=%d, writesize=%d, bufcnt=%d\n", 
			//	oaf->valid_samples, lis, samplesleft, writesize, bufcnt);
			memcpy( (buf+bufcnt), (void *) ( oaf->samples.f +  ((oaf->valid_samples - samplesleft)  * num_channels)), writesize );
			lis = lis - samplesleft;
			samplesleft = 0;
			totalwritesize += writesize;
			bufcnt += writesize;
			//printf("samplesleft > 0 oaf->vs %d, lis=%d  samplesleft=%d, bufcnt=%d\n", 
			//oaf->valid_samples, lis, samplesleft, bufcnt);
		} else { //samplesleft should be zero
			// no samples left, get a new frame
			//gavl_audio_frame_null(taf);
			if( !fifo->Get( taf ) ) {
				//printf("Couldn't get a frame\n");
				return totalwritesize / num_channels / bytes_per_sample;
			}
			
			lockA();
			timestamp = taf->timestamp;
			if ( do_t2o_convert  ) {
				//gavl_audio_convert( t2o_audio_converter, taf, oaf );
				gavl_audio_converter_resample( t2o_audio_converter, taf, oaf, src_factor );
				samplesleft = oaf->valid_samples;
				//printf("converting taf to oaf,  taf->vs %d, oaf->vs %d\n", taf->valid_samples, oaf->valid_samples);
			} else {
				// copy the samples to the output
				gavl_audio_frame_copy(&tmp_audio_format, oaf,  taf, 0,0, taf->valid_samples, taf->valid_samples) ;
				//printf("copying taf to oaf,  taf->vs %d, oaf->vs %d\n", taf->valid_samples, oaf->valid_samples);
				samplesleft = taf->valid_samples;
				oaf->valid_samples = taf->valid_samples;
			}
			unlockA();
		}
	
	}
	this->Signal();
	//printf("-----\n");
	ret = totalwritesize / num_channels / bytes_per_sample;
	if (ret != lengthinsamples) 
		printf("OUCH %d\n",  ret );
	return ret ;
}

bool Readsf::Rewind() {
	if (is_open) {
		iaf->valid_samples = 0;
		if (!doLoop())
			samplesleft = 0;
		PCM_seek( 0);
		eof = false;
		// fifo flush happens when we seek
		return true;
	}
	return false;
}

float Readsf::getLengthInSeconds() {
	gavl_time_t t;

	if (file != NULL) {
		t= bgav_get_duration ( file, 0);
		return (float)(gavl_time_to_samples( input_audio_format.samplerate, t) / input_audio_format.samplerate);	 
	}
	return 0.0;
}

void Readsf::setSpeed( float f) {
	if (is_open) {
		if (f > SRC_MAX) 
			return;
		if (f < SRC_MIN)
			return;
		//lockA();
		src_factor = f;
		output_audio_format.samplerate =  src_factor * tmp_audio_format.samplerate;
		//doT2OConvert( gavl_audio_converter_init( t2o_audio_converter, 
		//		&tmp_audio_format, &output_audio_format) ) ;
		//unlockA();
	}
}

bool Readsf::RewindNoFlush() {
	gavl_time_t gt = gavl_samples_to_time( (int)input_audio_format.samplerate, 0 ) ;        
	if (is_open) {
		if (bgav_can_seek ( file) ) {
			bgav_seek(this->file, &gt);
			return true;
		}
	}
	return false;
}
bool Readsf::PCM_seek(long samples) {
	gavl_time_t gt = gavl_samples_to_time( (int)input_audio_format.samplerate, (int64_t) samples ) ;        
	if (this->is_open) {
		if (bgav_can_seek ( this->file) ) {
			//bgav_seek_audio(x->file, 0, (int64_t) f);
			this->lockA();
			this->fifo->Flush();
			bgav_seek(this->file, &gt);
			this->unlockA();
			this->Signal();
			//post ("seeking %d", gt);
			return true;
		}
	}
	return false;
}

bool Readsf::TIME_seek(double seconds) {
  return false;
}

void Readsf::Open(char * fn) {
	if (state == STATE_STARTUP) {
		printf("Still opening a file what to do?\n");
	}
	state = STATE_STARTUP;
	is_open = false;
	// how can we be careful here of s_name's with commas in them
	printf("%s\n", fn);

	sprintf(filename, fn);
	fifo->Flush();
	//thread_open( (void *)this);
	pthread_create(&thr_open, NULL, thread_open, (void *)this);
}


void Readsf::setOptions() {
	if (opt == NULL)
		free (opt);
	opt = NULL;
	opt = bgav_get_options(file);

	bgav_options_set_connect_timeout(opt,  5000);
	bgav_options_set_read_timeout(opt,     5000);
	bgav_options_set_network_bandwidth(opt, 524300);
	bgav_options_set_network_buffer_size(opt, 1024*12);
	bgav_options_set_http_shoutcast_metadata (opt, 1);
	// set up the reading so that we can seek sample accurately
	// bgav_options_set_sample_accurate (rdsf->opt, 1 );
}

void Readsf::initFormat() {

	const gavl_audio_format_t * open_audio_format;

	// only concerned with the first audio stream
	open_audio_format = bgav_get_audio_format(file, 0);    

	gavl_audio_format_copy(&input_audio_format, open_audio_format);
	input_audio_format.samples_per_frame = SPF;

	if (iaf != NULL)
		gavl_audio_frame_destroy(iaf);

	iaf = gavl_audio_frame_create(&input_audio_format);

	setSRCFactor( output_audio_format.samplerate / (float) input_audio_format.samplerate); 
	
	//make sure the input samplerate is the same as the tmp samplerate
	tmp_audio_format.samplerate = input_audio_format.samplerate;
	
	//doT2OConvert( gavl_audio_converter_init( t2o_audio_converter, &tmp_audio_format, &output_audio_format) ) ;
	
	doT2OConvert( gavl_audio_converter_init_resample( t2o_audio_converter, &output_audio_format) ) ;

	doI2TConvert( gavl_audio_converter_init( i2t_audio_converter, 
				&input_audio_format, &tmp_audio_format) ) ;

	/*
	printf("\n-----------\ninput_audio_format\n");
	gavl_audio_format_dump ( &input_audio_format);

	printf("\n-----------\ntmp_audio_format\n");
	gavl_audio_format_dump ( &tmp_audio_format);

	printf("\n-----------\noutput_audio_format\n");
	gavl_audio_format_dump ( &output_audio_format);
	*/
}

void *thread_open(void *xp) {
	Readsf *rdsf = (Readsf *)xp;
	int num_urls, i, num_audio_streams;
	
	rdsf->lockA();
	
	rdsf->setFile();
	rdsf->setOptions();

	if(!bgav_open(rdsf->getFile(), rdsf->getFilename())) {
		printf( "Could not open file %s\n", rdsf->getFilename());
		rdsf->isOpen( false );
		//free(rdsf->getFile());
		rdsf->setState( STATE_EMPTY);
		rdsf->unlockA();
		return NULL;
	} else {
		rdsf->isOpen( true );
		printf("opened %s\n", rdsf->getFilename());
	}

	if(bgav_is_redirector(rdsf->getFile() )) {
		num_urls = bgav_redirector_get_num_urls(rdsf->getFile() );
		printf( "Found redirector with %d urls inside, we will try to use the first one.\n", num_urls);
		printf( "Name %d: %s\n", 1, bgav_redirector_get_name(rdsf->getFile() , 0));
		printf("URL %d: %s\n",  1, bgav_redirector_get_url(rdsf->getFile(), 0));
		if (!bgav_open( rdsf->getFile(),  bgav_redirector_get_url(rdsf->getFile(), 0))) {
			printf("Could not open redirector\n");
			rdsf->isOpen( false); 
			free(rdsf->getFile() );
			rdsf->setState( STATE_EMPTY );
			rdsf->unlockA();
			return NULL;
		} else {
			rdsf->isOpen(true);
			sprintf(rdsf->getFilename(), bgav_redirector_get_url(rdsf->getFile(), 0) );
			printf("opened redirector %s\n", rdsf->getFilename());
		}
	}
	//rdsf->num_tracks = bgav_num_tracks(rdsf->getFile());
	//track =0;
	bgav_select_track(rdsf->getFile(), 0);

	num_audio_streams = bgav_num_audio_streams(rdsf->getFile(), 0);
	for(i = 0; i < num_audio_streams; i++)
		bgav_set_audio_stream(rdsf->getFile(), i, BGAV_STREAM_DECODE);

	if(!bgav_start(rdsf->getFile())) {
		printf( "failed to start file\n");
		rdsf->setState( STATE_EMPTY);
		rdsf->unlockA();
		return NULL;
	}
	//bgav_dump(rdsf->getFile());
	
	rdsf->initFormat();

	rdsf->setState( STATE_READY);
	rdsf->unlockA();
	rdsf->Signal();
	rdsf->callOpenCallback();
	return NULL;
}

void *fill_fifo( void * xp) {
	int samples_returned;
	Readsf *rdsf = (Readsf *)xp;
	

	while (!rdsf->quit) {
		if (rdsf->isReady() ) {
			
			while ( rdsf->getFifo()->FreeSpace() ) {
				//printf("got a frame\n");	
				rdsf->lockA();
				samples_returned = bgav_read_audio(rdsf->getFile(), rdsf->getIAF(), 0,  SPF);

				if (samples_returned == 0 ) {
					if (rdsf->doLoop() ) {
						//printf("looping ...\n");
						rdsf->RewindNoFlush();
					} else {
						//printf("end of file \n");
						rdsf->setEOF(true);
					}
					//quit = true;
					rdsf->unlockA();
					break;
				}
				if (rdsf->doI2TConvert() ) {
					//printf("samps_returned %d, tmp_format spf %d\n", samples_returned, rdsf->getFifo()->getFormat()->samples_per_frame);
					gavl_audio_convert( rdsf->getI2TAudioConverter(), rdsf->getIAF(), rdsf->getTAF()) ;
					rdsf->getFifo()->Append( rdsf->getTAF() );
					//printf("taf->valid_samples %d\n", rdsf->getTAF()->valid_samples);
					
				} else {
					rdsf->getFifo()->Append( rdsf->getIAF() );
				}
				rdsf->unlockA();
			}
		}
		// wait for external signal to continue in this thread.
		//printf("waiting\n");	
		
		rdsf->Wait();
	}
	return NULL;

}

