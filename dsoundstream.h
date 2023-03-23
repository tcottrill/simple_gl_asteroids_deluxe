#pragma once
#define NOMINMAX
#include <Windows.h>
//#include <corerror.h>

void osd_set_mastervolume(int attenuation);
HRESULT stream_init(int sample_rate, int channels);
void osd_update_audio_stream(INT16 *buffer, int samples_this_frame);
void stream_stop();

