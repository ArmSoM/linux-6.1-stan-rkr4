#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include "slog.h"

static int pcm_set_hw_params(snd_pcm_t *pcm, int channels, int rate,
        unsigned int *buffer_time, unsigned int *period_time) {

    const snd_pcm_access_t access = SND_PCM_ACCESS_RW_INTERLEAVED;
    const snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    snd_pcm_hw_params_t *params;
    char buf[256];
    int dir;
    int err;

    snd_pcm_hw_params_alloca(&params);

    if ((err = snd_pcm_hw_params_any(pcm, params)) < 0) {
        snprintf(buf, sizeof(buf), "Set all possible ranges: %s", snd_strerror(err));
        goto fail;
    }
    if ((err = snd_pcm_hw_params_set_access(pcm, params, access)) != 0) {
        snprintf(buf, sizeof(buf), "Set assess type: %s: %s", snd_strerror(err), snd_pcm_access_name(access));
        goto fail;
    }
    if ((err = snd_pcm_hw_params_set_format(pcm, params, format)) != 0) {
        snprintf(buf, sizeof(buf), "Set format: %s: %s", snd_strerror(err), snd_pcm_format_name(format));
        goto fail;
    }
    if ((err = snd_pcm_hw_params_set_channels(pcm, params, channels)) != 0) {
        snprintf(buf, sizeof(buf), "Set channels: %s: %d", snd_strerror(err), channels);
        goto fail;
    }
    if ((err = snd_pcm_hw_params_set_rate(pcm, params, rate, 0)) != 0) {
        snprintf(buf, sizeof(buf), "Set sampling rate: %s: %d", snd_strerror(err), rate);
        goto fail;
    }
    if ((err = snd_pcm_hw_params_set_buffer_time_near(pcm, params, buffer_time, &dir)) != 0) {
        snprintf(buf, sizeof(buf), "Set buffer time: %s: %u", snd_strerror(err), *buffer_time);
        goto fail;
    }
    if ((err = snd_pcm_hw_params_set_period_time_near(pcm, params, period_time, &dir)) != 0) {
        snprintf(buf, sizeof(buf), "Set period time: %s: %u", snd_strerror(err), *period_time);
        goto fail;
    }
    if ((err = snd_pcm_hw_params(pcm, params)) != 0) {
        snprintf(buf, sizeof(buf), "%s", snd_strerror(err));
        goto fail;
    }

    return 0;

fail:
    ALOGE("%s\n", buf);
    return err;
}

static int pcm_set_sw_params(snd_pcm_t *pcm, snd_pcm_uframes_t buffer_size,
        snd_pcm_uframes_t period_size) {

    snd_pcm_sw_params_t *params;
    char buf[256];
    int err;

    snd_pcm_sw_params_alloca(&params);

    if ((err = snd_pcm_sw_params_current(pcm, params)) != 0) {
        snprintf(buf, sizeof(buf), "Get current params: %s", snd_strerror(err));
        goto fail;
    }

    /* start the transfer when the buffer is full (or almost full) */
    snd_pcm_uframes_t threshold = (buffer_size / period_size) * period_size;
    if ((err = snd_pcm_sw_params_set_start_threshold(pcm, params, threshold)) != 0) {
        snprintf(buf, sizeof(buf), "Set start threshold: %s: %lu", snd_strerror(err), threshold);
        goto fail;
    }

    /* allow the transfer when at least period_size samples can be processed */
    if ((err = snd_pcm_sw_params_set_avail_min(pcm, params, period_size)) != 0) {
        snprintf(buf, sizeof(buf), "Set avail min: %s: %lu", snd_strerror(err), period_size);
        goto fail;
    }

    if ((err = snd_pcm_sw_params(pcm, params)) != 0) {
        snprintf(buf, sizeof(buf), "%s", snd_strerror(err));
        goto fail;
    }

    return 0;

fail:
    ALOGE("%s\n", buf);
    return err;
}

const char * device = "hw:2,0";
int pcm_open(snd_pcm_t **pcm, const char* device, int channels, int rate,
        unsigned int *buffer_time, unsigned int *period_time) {
    snd_pcm_t *_pcm = NULL;
    char buf[256];
    char *tmp;
    int err;

    if ((err = snd_pcm_open(&_pcm, device, SND_PCM_STREAM_PLAYBACK, 0)) != 0) {
        snprintf(buf, sizeof(buf), "%s", snd_strerror(err));
        goto fail;
    }

    if ((err = pcm_set_hw_params(_pcm, channels, rate, buffer_time, period_time)) != 0) {
        snprintf(buf, sizeof(buf), "Set HW params: %s", "fail");
        goto fail;
    }

    snd_pcm_uframes_t buffer_size, period_size;
    if ((err = snd_pcm_get_params(_pcm, &buffer_size, &period_size)) != 0) {
        snprintf(buf, sizeof(buf), "Get params: %s", snd_strerror(err));
        goto fail;
    }

    if ((err = pcm_set_sw_params(_pcm, buffer_size, period_size)) != 0) {
        snprintf(buf, sizeof(buf), "Set SW params: %s", "fail");
        goto fail;
    }

    if ((err = snd_pcm_prepare(_pcm)) != 0) {
        snprintf(buf, sizeof(buf), "Prepare: %s", snd_strerror(err));
        goto fail;
    }

    *pcm = _pcm;
    return 0;

fail:
    if (_pcm != NULL)
        snd_pcm_close(_pcm);
    ALOGE("%s", buf);
    return err;
}

int pcm_close(snd_pcm_t **pcm) {
    if ((pcm == NULL) || (*pcm == NULL))
        return 0;
    snd_pcm_close(*pcm);
    *pcm = NULL;
    return 0;
}

