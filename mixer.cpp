#define NOMINMAX
#include "framework.h"
#include "mixer.h"
#include "wavfile.h"
#include "fileio.h"
#include "dsoundmain.h"
#include "dsoundstream.h"
#include <cstdint>
#include <math.h>
//#include "llist.h"

#include <list>
//#include <limits>
//#include <algorithm>    // std::max
//#include <exception> //static_assert

static int BUFFER_SIZE = 0;
//#define SMP_START 0x2c
#define MAX_CHANNELS   12 // 0-7 game, 8-9 system, 10-11 streaming
#define MAX_SOUNDS     130 // max number of sounds loaded in system at once 128 + 2 overloaded for streams
#define SOUND_NULL     0
#define SOUND_LOADED   1
#define SOUND_PLAYING  2
#define SOUND_STOPPED  3
#define SOUND_PCM      4
#define SOUND_STREAM   5

static double dbvolume[101];


inline float dBToAmplitude(float db)
{
	return pow(10.0f, db / 20.0f);
}

void buildvolramp() //This only goes down, not up. Need to build an up vol to 200 percent as well
{

	dbvolume[0] = 0;
	double k = 0;

	for (int i = 99; i > 0; i--)
	{
		k = k - .44f;
		dbvolume[i] = dBToAmplitude(k);
		wrlog("Value at %i is %f", i, dbvolume[i]);
	}

	dbvolume[100] = 1.00;

}

unsigned char Make8bit(signed short sample)
{
	sample >>= 8;  // drop the low 8 bits
	sample ^= 0x80;  // toggle the sign bit
	return (sample & 0xFF);
}

short Make16bit(unsigned char sample)
{
	short sample16 = (short)(sample - 0x80) << 8;
	return sample16;
}



short int *soundbuffer;
//short int *sstream[2];

CHANNEL channel[MAX_CHANNELS];
SAMPLE sound[MAX_SOUNDS];

std::list<int> audio_list;

static void byteswap(unsigned char & byte1, unsigned char & byte2)
{
	byte1 ^= byte2;
	byte2 ^= byte1;
	byte1 ^= byte2;
}

bool ends_with(const std::string& s, const std::string& ending)
{
	return (s.size() >= ending.size()) && equal(ending.rbegin(), ending.rend(), s.rbegin());
}

int load_sample(char *archname, char *filename)
{
	int	sound_id = -1;      // id of sound to be loaded
	int  index;               // looping variable
	// step one: are there any open id's ?
	for (index = 0; index < MAX_SOUNDS; index++)
	{
		// make sure this sound is unused
		if (sound[index].state == SOUND_NULL)
		{
			sound_id = index;
			break;
		} // end if

	} // end for index
	  // did we get a free id? If not,fail.
	if (sound_id == -1) {
		wrlog("No free sound id's for sample %s", filename); return(-1);
	}
	//SOUND

	wrlog("Loading file %s with sound id %d", filename, sound_id);


	unsigned char *sample_temp;
	HRESULT result;
	//LOAD FILE - Please add some error handling here!!!!!!!!!
	if (archname)
	{
		sample_temp = load_generic_zip(archname, filename);
		//Create Wav data
		result = WavFileLoadInternal(sample_temp, get_last_zip_file_size());
	}
	else
	{

		sample_temp = load_file(filename);
		//Create Wav data
		result = WavFileLoadInternal(sample_temp, get_last_file_size());
	}

	//If sample loaded successfully proceed!

	// set rate and size in data structure
	sound[sound_id].sampleRate = Wave.sampleRate;
	sound[sound_id].bitPerSample = Wave.bitPerSample;
	sound[sound_id].dataLength = Wave.dataLength;
	sound[sound_id].sampleCount = Wave.sampleCount;
	sound[sound_id].state = SOUND_LOADED;
	sound[sound_id].name = filename;

	wrlog("File %s loaded with sound id: %d and state is: %d", filename, sound_id, sound[sound_id].state);
	wrlog("Loading WAV #: %d", sound_id);
	wrlog("Channels #: %d", Wave.channels);
	wrlog("Samplerate #: %d", Wave.sampleRate);
	wrlog("Length #: %d", Wave.dataLength);
	wrlog("BPS #: %d", Wave.bitPerSample);
	wrlog("Samplecount #: %d", Wave.sampleCount);

	// Add rate/stereo conversion here.
	sound[sound_id].data.buffer = (unsigned char *)malloc(Wave.dataLength);
	memcpy(sound[sound_id].data.buffer, sample_temp + 0x2c, Wave.dataLength); //Have to cut out the header data from the wave data
	free(sample_temp);

	//Return Sound ID
	wrlog("Loaded sound success");
	return(sound_id);
}


void mixer_init(int rate, int fps) //Right now this is only going to work for 60 fps
{
	int i = 0;
	BUFFER_SIZE = rate / fps;
	soundbuffer = (short *)malloc(BUFFER_SIZE * 2);
	memset(soundbuffer, 0, BUFFER_SIZE * 2);
	dsound_init(rate, 1); //Start the directsound engine
	stream_init(rate, 1);  //Start a stream playing for our data
	osd_set_mastervolume(-3); //Set volume

	//Clear and init Sample Channels
	for (i = 0; i < MAX_CHANNELS; i++)
	{
		channel[i].loaded_sample_num = -1;
		channel[i].state = SOUND_STOPPED;
		channel[i].looping = 0;
		channel[i].pos = 0;
		channel[i].vol = 1.0;
		//sample_set_volume(i, 100);
		wrlog("Channel default volume is %f", channel[i].vol);
	}

	//Set all samples to empty for start
	for (i = 0; i < MAX_SOUNDS; i++)
	{
		sound[i].state = SOUND_NULL;
	}
	buildvolramp();
}


void mixer_update()
{
	int32_t smix = 0;    //Sample mix buffer
	int32_t fmix = 0;   // Final sample mix buffer
	

	for (int i = 0; i < BUFFER_SIZE; i++)
	{
		fmix = 0; //Set mix buffer to zero (silence) 

		for (std::list<int>::iterator it = audio_list.begin(); it != audio_list.end(); ++it)
		{
			
			SAMPLE p = sound[channel[*it].loaded_sample_num]; //To shorten 

			if (channel[*it].pos >= p.sampleCount)//p.dataLength)
			{
				if (channel[*it].looping == 0) { channel[*it].state = SOUND_STOPPED; audio_list.erase(it); }
				channel[*it].pos = 0; //? Always rewind sample? 
			}
			// 16 bit mono
			if (p.bitPerSample == 16)
			{

				smix = (short)p.data.u16[channel[*it].pos];
				smix = smix * channel[*it].vol;
				channel[*it].pos += 1;
			}
			// 8 bit mono
			else if (p.bitPerSample == 8)
			{
				smix = (short)(((p.data.u8[channel[*it].pos] - 128) << 8));
				smix = smix * channel[*it].vol;
				channel[*it].pos += 1;
			}
			
			smix = static_cast<int32_t> (smix >> 1);//(smix * .50); //Reduce volume to avoid clipping. This number can vary depending on the samples.?
													  //Mix here.
			fmix = fmix + smix;
			//}
		}
		if (fmix) //If the mix value is zero (nothing playing) , skip all this.
		{
			//Clip samples
			//16 bit maximum is 32767 (0x7fff)
			//16 bit minimum is -32768 (0x8000)
			if (fmix > 32767) fmix = 32767;
			if (fmix < -32768) fmix = -32768;
		}

		(short)soundbuffer[i] = static_cast<short>(fmix);
	}

	osd_update_audio_stream(soundbuffer, BUFFER_SIZE);
}


void mixer_end()
{
	dsound_stop();
	for (int i = 0; i < MAX_SOUNDS; i++)
	{
		if (sound[i].data.buffer)
		{
			free(sound[i].data.buffer);
			wrlog("Freeing sample #%d named %s", i, sound[i].name.c_str());
		}
	}
}


void sample_stop(int chanid)
{
	channel[chanid].state = SOUND_STOPPED;
	channel[chanid].looping = 0;
	channel[chanid].pos = 0;
	audio_list.remove(chanid);
}


void sample_start(int chanid, int samplenum, int loop)
{
	if (channel[chanid].state == SOUND_PLAYING)
	{
		wrlog("error, sound already playing on this channel %d state: %d", chanid, channel[chanid].state);
		return;
	}

	channel[chanid].state = SOUND_PLAYING;
	channel[chanid].stream_type = SOUND_PCM;
	channel[chanid].loaded_sample_num = samplenum;
	channel[chanid].looping = loop;
	channel[chanid].pos = 0;
	//channel[chanid].vol = 1.0;
	audio_list.emplace_back(chanid);
}


int sample_get_position(int chanid)
{
	return channel[chanid].pos;
}


void sample_set_volume(int chanid, int volume)
{
	/*
	if (volume == 0)
	{
		channel[chanid].vol = 0.526;
	}

	else
	{
		channel[chanid].vol = 1.0;
	}
	*/
	channel[chanid].vol = dbvolume[volume];

	wrlog("Setting channel %i to with volume %i setting bvolume %f", chanid, volume, channel[chanid].vol);

	//channel[chanid].vol = 100 * (1 - (log(volume) / log(0.5)));
	// vol = CLAMP(0, pow(10, (vol/2000.0))*255.0 - DSBVOLUME_MAX, 255);

	/*
	To increase the gain of a sample by X db, multiply the PCM value by pow( 2.0, X/6.014 ). i.e. gain +6dB means doubling the value of the sample, -6dB means halving it.
	inline double amp2dB(const double amp)
{
	// input must be positive +1.0 = 0dB
	if (amp < 0.0000000001) { return -200.0; }
	return (20.0 * log10(amp));
}
inline double dB2amp(const double dB)
{
  // 0dB = 1.0
  //return pow(10.0,(dB * 0.05)); // 10^(dB/20)
  return exp(dB * 0.115129254649702195134608473381376825273036956787109375);
}



0. (init) double max = 0.0; double tmp;
1. convert your integer audio to floating point (i would use double, but whatever)
2. for every sample do:
CODE: SELECT ALL

tmp = amp2dB(fabs(x));
max = (tmp > max ? tmp : max); // store the highest dB peak..
3. at the end, when you have processed the whole audio - max gives the maximum peak level in dB, so to normalize to 0dB you can do this:
0. (precalculate) double scale = dB2amp(max * -1.0);
1. multiply each sample by "scale"
2. convert back to integer if you want..

OR
float volume_control(float signal, float gain) {

	return signal * pow( 10.0f, db * 0.05f );

}

OR
Yes, gain is just multiplying by a factor. A gain of 1.0 makes no change to the volume (0 dB), 0.5 reduces it by a factor of 2 (-6 dB), 2.0 increases it by a factor of 2 (+6 dB).

To convert dB gain to a suitable factor which you can apply to your sample values:

double gain_factor = pow(10.0, gain_dB / 20.0);

OR
inline float AmplitudeTodB(float amplitude)
{
  return 20.0f * log10(amplitude);
}

inline float dBToAmplitude(float dB)
{
  return pow(10.0f, db/20.0f);
}

Decreasing Volume:

dB	Amplitude
-1	0.891
-3	0.708
-6	0.501
-12	0.251
-18	0.126
-20	0.1
-40	0.01
-60	0.001
-96	0.00002
Increasing Volume:

dB	Amplitude
1	1.122
3	1.413
6	1.995
12	3.981
18	7.943
20	10
40	100
60	1000
96	63095.734

	*/
};

int sample_get_volume(int chanid) { return 0; };
void sample_set_position(int chanid, int pos) {};
void sample_set_freq(int channid, int freq) {};


int sample_playing(int chanid)
{
	if (channel[chanid].state == SOUND_PLAYING)
		return 1;
	else return 0;
}


void sample_end(int chanid)
{
	channel[chanid].looping = 0;
}


void stream_start(int chanid, int stream)
{

	int stream_sample = create_sample(16, 0, 44100, BUFFER_SIZE);

	if (channel[chanid].state == SOUND_PLAYING)
	{
		wrlog("error, sound already playing on this channel %d state: %d", chanid, channel[chanid].state);
		return;
	}

	channel[chanid].state = SOUND_PLAYING;
	channel[chanid].loaded_sample_num = stream_sample;
	channel[chanid].looping = 1;
	channel[chanid].pos = 0;
	channel[chanid].stream_type = SOUND_STREAM;
	audio_list.emplace_back(chanid);

	/*
	if (stream == STREAM0)
	{
		channel[stream].loaded_sample_num = 128;
		sound[128].bitPerSample = 4;
		sound[128].dataLength = BUFFER_SIZE * 2;
		sstream[0] = (short *)malloc(BUFFER_SIZE * 2);
	}
	else
	{
		channel[stream].loaded_sample_num = 129;
		sound[129].bitPerSample = 4;
		sound[129].dataLength = BUFFER_SIZE * 2;
		sstream[1] = (short *)malloc(BUFFER_SIZE * 2);
	}
	channel[stream].looping = 1;
	channel[stream].pos = 0;
	*/
}


void stream_stop(int chanid, int stream)
{
	channel[stream].state = SOUND_STOPPED;
	channel[stream].loaded_sample_num = 0;
	channel[stream].looping = 0;
	channel[stream].pos = 0;
	audio_list.remove(chanid);
	//Warning, This doesn't delete the created sample	
}


void stream_update(int chanid, int stream, short *data)
{
	if (channel[chanid].state == SOUND_PLAYING)
	{
		SAMPLE p = sound[channel[chanid].loaded_sample_num];
		memcpy(p.data.buffer, data, p.dataLength);
	}

}

void sample_remove(int samplenum)
{

}



//soundbuffer[i] = ((short)(sound[j].data[sample_pos + y] << 8) | (sound[j].data[sample_pos + (y + 1)]));
/*
short int mix_sample(short int sample1, short int sample2) {
	const int32_t result(static_cast<int32_t>(sample1) + static_cast<int32_t>(sample2));
	typedef std::numeric_limits<short int> Range;
	if (Range::max() < result)
		return Range::max();
	else if (Range::min() > result)
		return Range::min();
	else
		return result;
}
*/


// create_sample:
// *  Constructs a new sample structure of the specified type.

int create_sample(int bits, int stereo, int freq, int len)
{
	wrlog("Buffer SIZE HERE IS %d", BUFFER_SIZE);
	int	sound_id = -1;      // id of sound to be loaded
	int  index;               // looping variable
	// step one: are there any open id's ?
	for (index = 0; index < MAX_SOUNDS; index++)
	{
		// make sure this sound is unused
		if (sound[index].state == SOUND_NULL)
		{
			sound_id = index;
			break;
		} // end if

	} // end for index
	  // did we get a free id? If not,fail.
	if (sound_id == -1) {
		wrlog("No free sound id's for creation of new sample?"); return(-1);
	}
	//SOUND
	wrlog("Creating Stream Audio with sound id %d", sound_id);
	// set rate and size in data structure
	sound[sound_id].sampleRate = freq;
	sound[sound_id].bitPerSample = bits;
	sound[sound_id].dataLength = BUFFER_SIZE * 2;
	sound[sound_id].sampleCount = BUFFER_SIZE;
	sound[sound_id].state = SOUND_LOADED;
	sound[sound_id].name = "STREAM";
	sound[sound_id].data.buffer = (unsigned char *)malloc(BUFFER_SIZE * 2);
	memset(sound[sound_id].data.buffer, 0, BUFFER_SIZE * 2);

	//spl->data = { (unsigned char *)malloc((len * ((bits == 8) ? 1 : sizeof(short)) * ((stereo) ? 2 : 1))) };

	return sound_id;
}



