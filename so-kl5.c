#include "so-kl5.h"


void runSO_kl5( LV2_Handle arg, uint32_t nframes ) {
	so_kl5* so=(so_kl5*)arg;
	lv2_event_begin(&so->in_iterator,so->MidiIn);
	
	double **strings=so->strings;
	unsigned int* stringpos=so->stringpos;
	unsigned int* stringlength=so->stringlength;
	double* tempstring=so->tempstring;
	double* stringcutoff=so->stringcutoff;
	float* outbuffer=so->output;
	int * status=so->status;

	int i, note;
	double  sample, damp;

	for( i=0; i<nframes; i++ ) {
		while(lv2_event_is_valid(&so->in_iterator)) {
			uint8_t* data;
			LV2_Event* event= lv2_event_get(&so->in_iterator,&data);
			if (event->type == 0) {
				so->event_ref->lv2_event_unref(so->event_ref->callback_data, event);
			} else if(event->type==so->midi_event_id) {
				if(event->frames > i) {
					break;
				} else{
					const uint8_t* evt=(uint8_t*)data;
					if((evt[0]&MIDI_CHANNELMASK)==so->channel) {
						if((evt[0]&MIDI_COMMANDMASK)==MIDI_NOTEON) 	{
							note = evt[1];
							if( ( note >= BASENOTE ) && ( note < BASENOTE+NUMNOTES ) )
							{
								note -= BASENOTE;

								status[note] = 1;
								
								int j;
								for( j=0; j<stringlength[note]; j++ )
								{
									tempstring[j] = ((double)rand()/(double)RAND_MAX)*2.0-1.0;
								}
								double velocity=evt[2];
								double freq = stringcutoff[note] * 0.25 + velocity/127.0 * 0.2 + so->sattack + 0.1;

								for( j=0; j<30; j++ )
								{
									tempstring[0] = tempstring[0]*freq + tempstring[stringlength[note]-1]*(1.0-freq);
									for( i=1; i<stringlength[note]; i++ )
									{
										tempstring[i] = tempstring[i]*freq + tempstring[(i-1)%stringlength[note]]*(1.0-freq);
									}
								}

								double avg = 0.0;

								for( j=0; j<stringlength[note]; j++ )
								{
									avg += tempstring[j];
								}

								avg /= stringlength[note];

								double scale = 0.0;

								for( j=0; j<stringlength[note]; j++ )
								{
									tempstring[j] -= avg;
									if( fabs( tempstring[j] ) > scale )
										scale = fabs( tempstring[j] );
								}

								double min = 10.0;
								int minpos = 0;

								for( j=0; j<stringlength[note]; j++ )
								{
									tempstring[j] /= scale;
									if( fabs( tempstring[j] ) + fabs( tempstring[j] - tempstring[j-1] ) * 5.0 < min )
									{
										min = fabs( tempstring[j] ) + fabs( tempstring[j] - tempstring[j-1] ) * 5.0;
										minpos = j;
									}
								}

								double vol = velocity/256.0;

								for( j=0; j<stringlength[note]; j++ )
								{
									strings[note][(stringpos[note]+j)%stringlength[note]] += tempstring[(j+minpos)%stringlength[note]]*vol;
								}
							}
						}
						else if((evt[0]&MIDI_COMMANDMASK)==MIDI_NOTEOFF )	{
							note = evt[1];
							if( ( note >= BASENOTE ) && ( note < BASENOTE+NUMNOTES ) )
							{
								note -= BASENOTE;
								status[note] = 0;
							}
						}
						else if((evt[0]&MIDI_COMMANDMASK)==MIDI_CONTROL )	{
							if( evt[1] == 74 )	{
								unsigned int cutoff =evt[2];
								so->fcutoff = (cutoff+5.0)/400.0;
								printf( "Cutoff: %i     \r", cutoff );
								fflush( stdout );
							} else if( evt[1]== 71 )	{
								unsigned int resonance = evt[2];
								so->freso = (resonance/160.0)*(1.0-so->fcutoff);
								printf( "Resonance: %i     \r", resonance );
								fflush( stdout );
							} else if( evt[1]== 73 )
							{
								unsigned int attack = evt[2];
								so->sattack = (attack+5.0)/800.0;
								printf( "Attack: %i     \r", attack );
								fflush( stdout );
							} else if( evt[1] == 7 )	{
								so->volume = evt[2];
								printf( "Volume: %i     \r", so->volume );
								fflush( stdout );
							} else if( evt[1]== 1 ||evt[1]==64) {
								unsigned int sustain =evt[2];
								so->ssustain = 0.6+pow( sustain/127.0, 0.4)*0.4;
								printf( "Sustain: %i    \r", sustain );
								fflush( stdout );
							}
						}
					}
				}
			}
			lv2_event_increment(&so->in_iterator);
		}
		sample = 0.0;

		for( note=0; note<NUMNOTES; note++ )
		{
			damp = stringcutoff[note];

			if( stringpos[note] > 0 )
				strings[note][stringpos[note]] = strings[note][stringpos[note]]*damp +
				strings[note][stringpos[note]-1]*(1.0-damp);
			else
				strings[note][stringpos[note]] = strings[note][stringpos[note]]*damp +
				strings[note][stringlength[note]-1]*(1.0-damp);

			damp = ((double)note/(double)NUMNOTES)*0.009999;

			if( status[note] == 0 )
				strings[note][stringpos[note]] *= 0.8+so->ssustain*0.19+damp;
			else
				strings[note][stringpos[note]] *= 0.99+damp;

			sample += strings[note][stringpos[note]];
		}

		so->hpval += (sample-(so->hplast)) * 0.00001;
		so->hplast += so->hpval;
		so->hpval *= 0.96;
		sample -= so->hplast;

		for( note=0; note<NUMNOTES; note++ )
		{
			damp = 1.0-((double)note/(double)NUMNOTES);
			strings[note][stringpos[note]] += sample*damp*0.001;

			if( fabs( strings[note][stringpos[note]] ) <= 0.00001 )
				strings[note][stringpos[note]] = 0.0;

			stringpos[note]++;
			if( stringpos[note] >= stringlength[note] ) stringpos[note] = 0;
		}

		so->lpval += (sample-so->lplast) * so->fcutoff;
		so->lplast += so->lpval;
		so->lpval *= so->freso;
		sample = so->lplast;

		outbuffer[i] = sample * (so->volume/127.0);
	}
}

LV2_Handle instantiateSO_kl5(const LV2_Descriptor *descriptor,double s_rate, const char *path,const LV2_Feature * const* features) {
	so_kl5* so=malloc(sizeof(so_kl5));
	LV2_URI_Map_Feature *map_feature;
	const LV2_Feature * const *  ft;
	for (ft = features; *ft; ft++) {
		if (!strcmp((*ft)->URI, "http://lv2plug.in/ns/ext/uri-map")) {
		            map_feature = (*ft)->data;
		            so->midi_event_id = map_feature->uri_to_id(
		                                                       map_feature->callback_data,
		                                                       "http://lv2plug.in/ns/ext/event",
		                                                       "http://lv2plug.in/ns/ext/midi#MidiEvent");
		                                                       } else if (!strcmp((*ft)->URI, "http://lv2plug.in/ns/ext/event")) {
		                                                                          so->event_ref = (*ft)->data;
															                                                                             }
	}

	puts( "SO-666 v.1.0 by 50m30n3 2009" );
	
	so->channel = 0;

	puts( "Initializing synth parameters" );
	unsigned int cutoff,resonance,sustain,attack;
	sustain = 0;
	cutoff = 64;
	resonance = 100;
	attack=4;
	so->volume = 100;

	so->lplast=0;
	so->lpval=0;
	so->hplast=0;
	so->hpval=0;
	
	so->fcutoff = (cutoff+5.0)/400.0;
	so->sattack = (attack+5.0)/800.0;
	so->freso = (resonance/160.0)*(1.0-so->fcutoff);
	so->ssustain = 0.6+pow( sustain/127.0, 0.4);
	
	int note;
	for( note=0; note<NUMNOTES; note++ ) {
		double freq = 440.0*pow( 2.0, (note+BASENOTE-69) / 12.0 );
		so->stringcutoff[note] = 0.3 + pow( (double)note / (double)NUMNOTES, 0.5 ) * 0.65;
		int length = round( (double)s_rate / freq );
		so->stringlength[note] = length;
		so->strings[note] = malloc( length * sizeof( double ) );
		if( so->strings[note] == NULL )
		{
			fputs( "Error allocating memory\n", stderr );
			return 0;
		}
		int i;
		for( i=0; i<length; i++ )
		{
			so->strings[note][i] = 0.0;
		}
		so->stringpos[note] = 0;
		so->status[note] = 0;
	}
	
	double freq = 440.0*pow( 2.0, (BASENOTE-69) / 12.0 );
	double length = (double)s_rate / freq;
	so->tempstring = malloc( length * sizeof( double ) );
	if( so->tempstring == NULL )
	{
		fputs( "Error allocating memory\n", stderr );
		return 0;
	}

	return so;
}
void cleanupSO_kl5(LV2_Handle instance) {
	so_kl5* so=(so_kl5*)instance;
	puts( "Freeing data");
	free(so->tempstring);
	int note;
	for(note=0; note<NUMNOTES; note++ )
	{
		free( so->strings[note] );
	}
	free(so);
}

void connectPortSO_kl5(LV2_Handle instance, uint32_t port, void *data_location) {
	so_kl5* so=(so_kl5*) instance;
	switch(port) {
		case PORT_OUTPUT:
			so->output=data_location;
			break;
		case PORT_MIDI:
			so->MidiIn=data_location;
			break;
		default:
			fputs("Warning, unconnected port!",stderr);
	}
}