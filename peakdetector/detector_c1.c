/* Geiger counter listener prototype - 2012
 * by "Cyrus Smith" for "Le Projet Olduvaï"
 *
 * See http://le-projet-olduvai.wikiforum.net/t6044-projet-de-logiciel-pour-compteur-geiger-muller
 *
 * This code is under GNU GPLv3.
 *
 * This is a simple detection algorithm based on the detection of an increase
 * of the amplitude following by a decrease, that is then considered as a peak.
 * A noise threshold is used to filter out small peaks, and multiple peak
 * detected during the Geiger dead time are ignored as an artefact.
 *
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "detector_c1.h"

/* This structure will hold data to and from the detection algorithm. */
struct detectordata {
	uint64_t last_peak_spl; /* last peak was at... */
	uint64_t sample_number; /* count the number of samples (= time) */
	uint32_t sample_rate;
	int32_t threshold; // detection threshold to filter noise
	double geiger_dead_time; /* Geiger dead time */
	int16_t last_values[2]; /* remember the last values between calls to the
				   callback fct */
	void (*detection_cb)(double, int16_t); /* callback to use when a peak
						  is detected */
};


/* More or less an extremely crude derivative function... */
static int8_t
evolution(int32_t value1, int32_t value2, int threshold)
{
	int32_t ret = (value2 - value1) / threshold;
	if (ret > INT8_MAX) {
		ret = INT8_MAX;
	}
	if (ret < INT8_MIN) {
		ret = INT8_MIN;
	}
	return (int8_t) ret;
}


/* Naive peak detection (both positive and negative ones).
   Note that, as this function is called for every value,
   the call to evolution(a, b, th) is the same as the call
   to evolution(b, c, th) of the previous call to peakp().
   This could hence be optimized if needed.
 */
static bool
peakp(int16_t a, int16_t b, int16_t c, int th)
{
	int8_t le = evolution(a, b, th);
	int8_t ce = evolution(b, c, th);

	/* Positive peaks */
	if (le >= 0 && ce < 0 && b > th)
		return true;

#if 0
	/* Negative peaks */
	if (le <= 0 && ce > 0 && b < -th)
		return true;
#endif
	return false;
}

static bool
dead_timep(struct detectordata *data)
{
	return false;

/// disabled

	uint64_t spl_diff = data->sample_number - data->last_peak_spl;
	double time_diff = ((double) spl_diff) / data->sample_rate;
	if (time_diff >= data->geiger_dead_time)
		return false;
	return true;
}

static int
detector(int16_t *in, size_t inputsize, struct detectordata *data)
{
	int16_t prev0 = data->last_values[0];
	int16_t prev1 = data->last_values[1];
	for (int i = 0; i < inputsize; i++) {
		data->sample_number++;
		if (peakp(prev0, prev1, in[i], data->threshold)) {
			if (!dead_timep(data)) {
				double time = ((double) data->sample_number)
					/ data->sample_rate;
				data->detection_cb(time, prev1);
				data->last_peak_spl = data->sample_number - 1;
			}
		}
		prev0 = prev1;
		prev1 = in[i];

	}
	data->last_values[0] = prev0;
	data->last_values[1] = prev1;

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
init_detector_c1(uint32_t sample_rate, const struct parameters *params,
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

	d->name = "C1";
	d->detector = &detector;
	d->terminate = &terminate_detector;
	d->data->sample_rate = sample_rate;
	d->data->threshold = params->noise_threshold;
	d->data->geiger_dead_time = params->geiger_dead_time;
	d->data->detection_cb = callback;
	return d;
}

