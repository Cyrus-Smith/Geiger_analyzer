/* Geiger counter listener prototype - 2012
 * by "Cyrus Smith" for "Le Projet Olduvaï"
 *
 * See http://le-projet-olduvai.wikiforum.net/t6044-projet-de-logiciel-pour-compteur-geiger-muller
 *
 * This is an implementation of the PPP algorithm (PapaPoilut's Peak)
 *
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "detector_ppp.h"

/* This structure holds data to and from the detection algorithm. */
struct detectordata {
	uint64_t last_peak_spl;  /* last peak was at... */
	uint64_t sample_number;  /* count the number of samples (= time) */
	uint32_t sample_rate;
	int32_t threshold;       /* detection threshold to filter noise */
	double geiger_dead_time; /* Geiger dead time */
	bool state;              /* true: during a peak; false: not a peak */
	void (*detection_cb)(double, int16_t); /* callback to use when a peak
						  is detected */
};

static bool
newpeakp(int16_t val, struct detectordata *data)
{
	assert(data != NULL);
	if (data->state && val < data->threshold) {
		data->state = false;
		return false;
	}
	if (!data->state && val > data->threshold) {
		data->state = true;
		return true;
	}
	return false;
}

static bool
dead_timep(struct detectordata *data)
{
	uint64_t spl_diff = data->sample_number - data->last_peak_spl;
	double time_diff = ((double) spl_diff) / data->sample_rate;
	if (time_diff >= data->geiger_dead_time)
		return false;
	return true;
}

static int
detector(int16_t *in, size_t inputsize, struct detectordata *data)
{
	for (int i = 0; i < inputsize; i++) {
		data->sample_number++;
		if (newpeakp(in[i], data)) {
			if (!dead_timep(data)) {
				double time = ((double) data->sample_number)
					/ data->sample_rate;
				data->detection_cb(time, in[i]);
				data->last_peak_spl = data->sample_number;
			}
		}
	}
	return 0;
}

static int
terminate_detector(struct detector* d)
{
	assert(d != NULL);
	assert(d->data != NULL);
	free(d->data);
	free(d);
	return 0;
}

struct detector*
init_detector_ppp(uint32_t sample_rate, const struct parameters *params,
	      void (*callback)(double, int16_t))
{
	assert(params != NULL);
	struct detector *d = (struct detector*)
		calloc(1, sizeof(struct detector));
	if (d == NULL)
		return NULL;
	d->data = (struct detectordata *)
		calloc(1, sizeof(struct detectordata));
	if (d->data == NULL) {
		free(d);
		return NULL;
	}

	d->name = "PPP";
	d->detector = &detector;
	d->terminate = &terminate_detector;
	d->data->sample_rate = sample_rate;
	d->data->threshold = params->noise_threshold;
	d->data->geiger_dead_time = params->geiger_dead_time + 0.0003;
	d->data->detection_cb = callback;
	return d;
}


