#include "clip.h"
#include "config.h"
#include "debug.h"
#include "global.h"

#include <jack/ringbuffer.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static void * clip_read (void * arg) {

    audio_clip_t * clip = (audio_clip_t*) arg;

    sf_count_t             buf_avail;
    sf_count_t             read_frames;
    jack_ringbuffer_data_t write_vector[2];

    size_t bytes_per_frame = FRAME_SIZE;

    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_mutex_lock(&clip->lock);

    while (1) {

        jack_ringbuffer_get_write_vector(clip->ringbuf, write_vector);

        read_frames = 0;

        if (write_vector[0].len) {

            buf_avail = write_vector[0].len / bytes_per_frame;

            read_frames = sf_readf_float(
                clip->sndfile,
                (float*) write_vector[0].buf,
                buf_avail);

        }

        if (read_frames == 0) break;

        jack_ringbuffer_write_advance(clip->ringbuf, read_frames * bytes_per_frame);

        clip->read_state = CLIP_READ_STARTED;

        pthread_cond_wait(&clip->ready, &clip->lock);

    }

    clip->read_state = CLIP_READ_DONE;

}

clip_index_t clip_add(global_state_t * context,
                      const char     * filename) {

    audio_clip_t * clip = calloc(1, sizeof(audio_clip_t));
    
    clip->read_state = CLIP_READ_INIT;
    clip->play_state = CLIP_STOP;
    clip->sfinfo     = calloc(1, sizeof(SF_INFO));
    clip->sndfile    = sf_open(filename, SFM_READ, clip->sfinfo);

    if (clip->sndfile == NULL) {
        FATAL("Could not open %s", filename);
        exit(1);
    }

    clip->filename = filename;

    MSG("%s: %d channels, %d kHz, %d frames, read %d, play %d",
        clip->filename,
        clip->sfinfo->channels,
        clip->sfinfo->samplerate,
        clip->sfinfo->frames,
        clip->read_state,
        clip->play_state);

    clip->ringbuf = jack_ringbuffer_create(
        sizeof(jack_default_audio_sample_t) * RINGBUFFER_SIZE);
    memset(clip->ringbuf->buf, 0, clip->ringbuf->size);

    context->clips[0] = clip;

    pthread_mutex_init(&clip->lock, NULL);
    pthread_cond_init(&clip->ready, NULL);
    pthread_create(&clip->thread, NULL, clip_read, clip);

    return 0;

}

void clip_start(global_state_t * context,
                clip_index_t     index) {

    context->clips[0]->play_state = CLIP_PLAY;

}
