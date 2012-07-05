
/* Geiger counter listener prototype - 2012-07-04
 * by "Cyrus Smith" for "Le Projet Olduvaï"
 * See http://le-projet-olduvai.wikiforum.net
 *
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <portaudio.h>


#define SAMPLE_RATE (44100)


/* This structure will hold data to and from the counting algorithm.
   An instance of this structure is used with the callback function
   that process the audio input. */
struct countdata {
	int count;
	int threshold; // detection threshold to filter noise
	int16_t last_values[2]; /* remember the last values between calls to the
				   callback fct */
	uint64_t sample_number; /* count the number of samples (= time) */
	PaTime initial_time;
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
   note that as this function is called for every value,
   the call to evolution(a, b, th) is the same as the call
   to evolution(b, c, th) of the previous call to peakp().
   This could hence be optimized if needed.
 */
static bool
peakp(int16_t a, int16_t b, int16_t c, int th)
{
	int8_t le = evolution(a, b, th);
	int8_t ce = evolution(b, c, th);
	if (le >= 0 && ce < 0 && b > th)
		return true;
	if (le <= 0 && ce > 0 && b < -th)
		return true;
	return false;
}

static int
geiger_callback(const void *input, void *output, unsigned long frameCount,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags status, void *ourData)
{
	static bool first = true;
	static PaTime initial_time;
	if (first) {
		initial_time = timeInfo->inputBufferAdcTime;
		first = false;
	}
	struct countdata *data = (struct countdata*) ourData;

	int16_t *in = (int16_t*) input;
	(void) output; /* Prevent unused variable warning. */

	if (status & paInputOverflow)
		fprintf(stderr, "Warning: input overflow\n");

	int16_t prev0 = data->last_values[0];
	int16_t prev1 = data->last_values[1];
	PaTime time = timeInfo->inputBufferAdcTime - initial_time;
	for(int i = 0; i < frameCount; i++) {
		if (peakp(prev0, prev1, in[i], data->threshold)) {
			printf("%.4f\t%6d\n", time + (double) i / SAMPLE_RATE,
			       prev1);
		}
		prev0 = prev1;
		prev1 = in[i];
		data->sample_number++;
	}
	data->last_values[0] = prev0;
	data->last_values[1] = prev1;

	return 0;
}



int
main(int argc, char *argv[])
{
	PaDeviceIndex dev_used = 0; /* To be initialized: will allow selection
				       of the soundcard / audio device used. */
	int threshold = 4; /* The detection threshold should be set to a "good"
			      default value, with a way for the user to specify
			      a different value. */


	/* Initialize portaudio */

	PaError perr = Pa_Initialize();
	if (perr != paNoError) {
		fprintf(stderr, "Pa_Initialize failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}

	/* For fun: display the available audiodevices (= soundcards?) */

	PaDeviceIndex nb_dev = Pa_GetDeviceCount();
	assert(nb_dev >= 0);
	fprintf(stderr, "%d device(s) found\n", nb_dev);

	for (int i = 0; i < nb_dev; i++) {
		const PaDeviceInfo* dev = Pa_GetDeviceInfo(i);
		assert(dev != NULL);
		fprintf(stderr, "device %d: %s; %d input channel(s)\n", i + 1,
			dev->name, dev->maxInputChannels);
	}




	struct countdata cdata = {0, threshold, {0,0}, 0, 0.0};


	/* open the input stream */
	PaStreamParameters stream_params = { dev_used, 1, paInt16,
					     Pa_GetDeviceInfo(dev_used)->defaultLowInputLatency,
					     NULL };
	PaStream *stream;
	perr = Pa_OpenStream(&stream, &stream_params, NULL, SAMPLE_RATE,
			     paFramesPerBufferUnspecified, paNoFlag, geiger_callback,
			     &cdata);
	if (perr != paNoError) {
		fprintf(stderr, "Pa_OpenStream failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}



	/* Temporary test: listen & process the audio input for 2 seconds. */

	perr = Pa_StartStream(stream);
	if (perr != paNoError) {
		fprintf(stderr, "Pa_StartStream failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}

	Pa_Sleep(2 * 1000);

	perr = Pa_StopStream(stream);
	if (perr != paNoError) {
		fprintf(stderr, "Pa_StopStream failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}


	fprintf(stderr, "%lu samples\n", cdata.sample_number);

	/* Close the stream and terminate properly portaudio. */

	perr = Pa_CloseStream(stream);
	stream = NULL;
	if (perr != paNoError) {
		fprintf(stderr, "Pa_CloseStream failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}

	perr = Pa_Terminate();
	if (perr != paNoError) {
		fprintf(stderr, "Pa_Terminate failed: %s\n",
			Pa_GetErrorText(perr));
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
