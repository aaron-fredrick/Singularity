#include "audio.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static SDL_AudioDeviceID audio_device = 0;
static double phase = 0.0;
static double current_freq = 440.0;
static double target_freq = 440.0;
static double current_amp = 0.0;
static double target_amp = 0.0;
static int is_enabled = 0;

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int len) {
    (void)userdata;
    float *fstream = (float *)stream;
    int samples = len / sizeof(float);
    
    for (int i = 0; i < samples; i++) {
        /* Smooth parameter transitions to avoid clicking */
        current_freq += (target_freq - current_freq) * 0.005;
        current_amp += (target_amp - current_amp) * 0.005;
        
        if (is_enabled && current_amp > 0.001) {
            fstream[i] = (float)(sin(phase) * current_amp * 0.2); // 0.2 limits max volume safely
            phase += 2.0 * M_PI * current_freq / 44100.0;
            if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
        } else {
            fstream[i] = 0.0f;
        }
    }
}

void audio_init(void) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = 44100;
    want.format = AUDIO_F32;
    want.channels = 1;
    want.samples = 1024;
    want.callback = audio_callback;
    
    audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 0);
    } else {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
    }
}

void audio_shutdown(void) {
    if (audio_device != 0) {
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }
}

void audio_set_params(double freq, double amp) {
    target_freq = freq;
    if (target_freq < 20.0) target_freq = 20.0;
    if (target_freq > 2000.0) target_freq = 2000.0;
    
    target_amp = amp;
    if (target_amp < 0.0) target_amp = 0.0;
    if (target_amp > 1.0) target_amp = 1.0;
}

void audio_set_enabled(int enabled) {
    is_enabled = enabled;
}

void audio_get_params(double *freq, double *amp) {
    *freq = current_freq;
    *amp = current_amp;
}
