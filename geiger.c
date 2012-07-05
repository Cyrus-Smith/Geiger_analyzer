
/* Geiger counter listener prototype - 2012
 * by "Cyrus Smith" for "Le Projet Olduvaï"
 *
 * See http://le-projet-olduvai.wikiforum.net/t6044-projet-de-logiciel-pour-compteur-geiger-muller
 *
 * This code is under GNU GPLv3.
 */

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <portaudio.h>


#define SAMPLE_RATE (44100)


/* This structure will hold data to and from the counting algorithm.
   An instance of this structure is used with the callback function
   that process the audio input. */
struct countdata {
	PaTime start_time;
	uint64_t count;
	int threshold; // detection threshold to filter noise
	int16_t last_values[2]; /* remember the last values between calls to the
				   callback fct */
	PaTime last_spl_time;   /* timestamp for the last sample processed */
	uint64_t sample_number; /* count the number of samples (= time) */
};
static const struct countdata init_cd = {0.0, 0, 0, {0,0}, 0.0, 0};


/* Signal Handling */

static sig_atomic_t quit = 0; // will be set to !0 by a signal asking to quit.

static void
exithandler(int signal)
{
	quit = 1;
}


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

static int
geiger_callback(const void *input, void *output, unsigned long frameCount,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags status, void *ourData)
{
	static bool first = true;
	struct countdata *data = (struct countdata*) ourData;
	if (first) {
		data->start_time = timeInfo->inputBufferAdcTime;
		first = false;
	}

	int16_t *in = (int16_t*) input;
	(void) output; /* Prevent unused variable warning. */

	if (status & paInputOverflow)
		fprintf(stderr, "Warning: input overflow\n");

	// fprintf(stderr, "fC: %lu\n", frameCount);

	int16_t prev0 = data->last_values[0];
	int16_t prev1 = data->last_values[1];
	PaTime time = timeInfo->inputBufferAdcTime - data->start_time;
	for (int i = 0; i < frameCount; i++) {
		/* The actual sample rate seems to be only an approximation of
		   SAMPLE_RATE, hence 'time + (double) i / SAMPLE_RATE' is only
		   an estimation of the sampling time. */
		if (peakp(prev0, prev1, in[i], data->threshold)) {
			data->count++;
			printf("%.4f\t%6d\n", time + (double) i / SAMPLE_RATE,
			       prev1);
		}
		prev0 = prev1;
		prev1 = in[i];
		data->sample_number++;
	}
	data->last_values[0] = prev0;
	data->last_values[1] = prev1;
	data->last_spl_time = time + (frameCount - 1.0) / SAMPLE_RATE;

	return 0;
}


/* Rate smoothing over interv_duration seconds. The struct smooth_counter is
   used with the smooth_rate function. As the smooth_rate display itself the
   rate every interv_duration period, it doesn't return anything, but we could
   imagine this function returning the rate. The rate implementation should
   then probably be modified to use a sliding period. */

struct smooth_counter {
	const int interv_duration;
	PaTime interv_start;
	uint64_t interv_count;
};

void
smooth_rate(struct smooth_counter *sc, struct countdata *data)
{
	sc->interv_count += data->count;
	double duration = data->last_spl_time - sc->interv_start;
	if (duration >= sc->interv_duration) {
		fprintf(stderr, "current rate over %.1f seconds: %.1f CPM\n",
			duration, 60 * sc->interv_count / duration);

		sc->interv_count = 0;
		sc->interv_start = data->last_spl_time;
	}

}



/* This is a placeholder for a function called regularly that could be used
   to process or display the new data. Only sample code currently. */
static void
process_new_data(struct countdata *data)
{
	static uint64_t total_count;
	static PaTime prev_time;


	static struct smooth_counter c10 = {10, 0, 0};
	smooth_rate(&c10, data);

	static struct smooth_counter c60 = {60, 0, 0};
	smooth_rate(&c60, data);

	static struct smooth_counter c3600 = {3600, 0, 0};
	smooth_rate(&c3600, data);



	if (data->count == 0)
		return;
	total_count += data->count;


	// rate between two calls (unless there is no new impulsion).
	// fprintf(stderr, "current rate: %.0f CPM\n",
	//	60 * data->count / (data->last_spl_time - prev_time));


	prev_time = data->last_spl_time;
	data->count = 0;
}


static void
list_sound_devices(void)
{
	PaDeviceIndex nb_dev = Pa_GetDeviceCount();
	assert(nb_dev >= 0);
	fprintf(stderr, "%d device(s) found\n", nb_dev);

	for (int i = 0; i < nb_dev; i++) {
		const PaDeviceInfo* dev = Pa_GetDeviceInfo(i);
		assert(dev != NULL);
		fprintf(stderr, "device %d: %s; %d input channel(s)\n", i + 1,
			dev->name, dev->maxInputChannels);
	}
}

static void
global_init(void)
{
	{ /* Redirecting SIGINT & SIGTERM */
		void (*oldh)();
		oldh = signal(SIGINT, &exithandler);
		if (oldh == SIG_ERR) {
			fprintf(stderr, "signal failed to redirect INT signal:"
				" %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		oldh = signal(SIGTERM, &exithandler);
		if (oldh == SIG_ERR) {
			fprintf(stderr, "signal failed to redirect TERM signal:"
				" %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* Initialize portaudio */

	PaError perr = Pa_Initialize();
	if (perr != paNoError) {
		fprintf(stderr, "Pa_Initialize failed: %s\n",
			Pa_GetErrorText(perr));
		exit(EXIT_FAILURE);
	}
}

static void
global_finishup(void)
{
	PaError perr = Pa_Terminate();
	if (perr != paNoError) {
		fprintf(stderr, "Pa_Terminate failed: %s\n",
			Pa_GetErrorText(perr));
		exit(EXIT_FAILURE);
	}
}

int
main(int argc, char *argv[])
{
	PaDeviceIndex dev_used = 0; /* To be initialized: will allow selection
				       of the soundcard / audio device used. */
	int threshold = 5; /* The detection threshold should be set to a "good"
			      default value, with a way for the user to specify
			      a different value. */

	global_init();

	/* For fun: display the available audiodevices (= soundcards?) */
	list_sound_devices();

	PaError perr;
	PaStream *stream;
	struct countdata cdata = init_cd;
	cdata.threshold = threshold;
	{ /* Open the input stream. */
		PaTime latency = Pa_GetDeviceInfo(dev_used)->defaultLowInputLatency;
		PaStreamParameters stream_params = { dev_used, 1, paInt16,
						     latency, NULL };

		perr = Pa_OpenStream(&stream, &stream_params, NULL, SAMPLE_RATE,
				     paFramesPerBufferUnspecified, paNoFlag,
				     geiger_callback, &cdata);
		if (perr != paNoError) {
			fprintf(stderr, "Pa_OpenStream failed: %s\n",
				Pa_GetErrorText(perr));
			return EXIT_FAILURE;
		}

		perr = Pa_StartStream(stream);
		if (perr != paNoError) {
			fprintf(stderr, "Pa_StartStream failed: %s\n",
				Pa_GetErrorText(perr));
			return EXIT_FAILURE;
		}
	}


	/* listen & process the audio input until we are asked to exit. */

	time_t t0 = time(NULL);

	while(!quit) {
		Pa_Sleep(500);
		process_new_data(&cdata);
	}

	time_t t1 = time(NULL);

	/* Dump some last accounting data, close the stream, finishup & exit. */

	double time = difftime(t1, t0);

	fprintf(stderr, "%lu samples processed in %.0f seconds (%f spl/s)\n",
		(long unsigned int) cdata.sample_number, time,
		cdata.sample_number/time);

	perr = Pa_StopStream(stream);
	if (perr != paNoError) {
		fprintf(stderr, "Pa_StopStream failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}

	perr = Pa_CloseStream(stream);
	stream = NULL;
	if (perr != paNoError) {
		fprintf(stderr, "Pa_CloseStream failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}

	global_finishup();

	return EXIT_SUCCESS;
}
