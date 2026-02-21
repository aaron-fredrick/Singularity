#ifndef AUDIO_H
#define AUDIO_H

#include <SDL.h>

/* Initialize SDL audio and start the callback thread */
void audio_init(void);

/* Shutdown the audio device */
void audio_shutdown(void);

/* Smoothly update the target frequency and amplitude.
   freq: in Hz. amp: 0.0 to 1.0. */
void audio_set_params(double freq, double amp);

/* Toggle synthesis on/off */
void audio_set_enabled(int enabled);

void audio_get_params(double *freq, double *amp);

#endif /* AUDIO_H */
