/* Geiger counter listener prototype - 2012
 * by "Cyrus Smith" for "Le Projet Olduvaï"
 *
 * See http://le-projet-olduvai.wikiforum.net/t6044-projet-de-logiciel-pour-compteur-geiger-muller
 *
 * This code is under GNU GPLv3.
 *
 * This program is to be used as part of a toolchain to detect "peaks",
 *  i.e., impulsions from a radiation detector.
 *
 * Use for instance with SoX [1]:
 *    $ sox -d -t wav -c 1 - | peakdetector
 * to analyse the audio stream from the soundcard.
 * Alternatively, to analyse a file, use:
 *    $ peakdetector file.wav
 * which is similar to:
 *    $ cat file.wav | peakdetector
 *
 * [1]: SoX: http://sox.sourceforge.net/
 *
 * The actual detection algorithm is implemented in another file and must
 * respect the interface provided by detector.h. It can then be changed
 * easily at compile time.
 *
 * WARNING: this program should not be used to detect peak oil and other
 *   ressource peaks.
 *
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sndfile.h>

#include "peakdetector.h"
#include "detector_c1.h"
#include "detector_ppp.h"

enum detectors {
	C1,
	PPP,
};

struct detector* (*detecinit[])(uint32_t sample_rate,
				const struct parameters *params,
				void (*callback)(double, int16_t)) = { &init_detector_c1, &init_detector_ppp };

static void
closeaudiostream(SNDFILE* stream)
{
	assert(stream != NULL);
	int err = sf_close(stream);
	if (err) {
		fprintf(stderr, "Unable to close properly audio stream: %s\n",
			sf_error_number(err));
		exit(EXIT_FAILURE);
	}
}

static SNDFILE*
openaudiostream(const char* filename, SF_INFO *sinfo)
{
	assert(filename != NULL);
	assert(sinfo != NULL);
	memset(sinfo, 0, sizeof sinfo);
	SNDFILE* stream = sf_open(filename, SFM_READ, sinfo);
	if (stream == NULL) {
		fprintf(stderr, "Unable to open audio stream: %s\n",
			sf_strerror(NULL));
		exit(EXIT_FAILURE);
	}

	if (sinfo->channels != 1) {
		fprintf(stderr, "Don't know how to process stream with %d "
			"channels\n", sinfo->channels);
		closeaudiostream(stream);
		exit(EXIT_FAILURE);

	}

	if (sinfo->format != (SF_FORMAT_WAV | SF_FORMAT_PCM_16)) {
		fprintf(stderr, "Input is not a 16-bit PCM WAV file\n");
		closeaudiostream(stream);
		exit(EXIT_FAILURE);
	}

	return stream;
}

void
displaycallback(double time, int16_t amplitude)
{
	printf("%.4f\t%6d\n", time, amplitude);
}

static void
usage(void)
{
	fprintf(stderr, "usage: peakdetector algorithm [inputfile]\n");
	fprintf(stderr, "\t algorithm can be C1 or PPP\n");
}

int
main(int argc, char *argv[])
{
	/* threshold, Geiger dead time */
	const struct parameters params = {500, 0.001};

	enum detectors detector;
	{
		if (argc < 2) {
			usage();
			exit(EXIT_FAILURE);
		}
		int av1len = strlen(argv[1]);

		if (av1len == 2 && !strncmp(argv[1], "C1", 2)) {
			detector = C1;
		} else if (av1len == 3 && !strncmp(argv[1], "PPP", 3)) {
			detector = PPP;
		} else {
			usage();
			exit(EXIT_FAILURE);
		}
	}

	SF_INFO sinfo;
	SNDFILE* stream = NULL;
	{
		char *filename;
		if (argc < 3)
			filename = "-";
		else
			filename = argv[2];

		stream = openaudiostream(filename, &sinfo);
		assert(stream != NULL);
	}

	struct detector *d;
	d = detecinit[detector](sinfo.samplerate, &params, &displaycallback);
	if (d == NULL) {
		fprintf(stderr, "Detector initialization failed\n");
		closeaudiostream(stream);
		return EXIT_FAILURE;
	}

	fprintf(stderr, "Using detection algorithm %s\n", d->name);
	fprintf(stderr, "Sample rate: %d\n", sinfo.samplerate);

	const size_t spl_size = 128;
	int16_t buffer[spl_size];

	while(1) {
		int nbfr = sf_readf_short(stream, buffer, spl_size);
		if (!nbfr)
			break;
		d->detector(buffer, nbfr, d->data);
	}

	d->terminate(d);

	closeaudiostream(stream);

	return EXIT_SUCCESS;
}
