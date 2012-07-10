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
#include "detector.h"

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
	return stream;
}

void
displaycallback(double time, int16_t amplitude)
{
	printf("%.4f\t%6d\n", time, amplitude);
}

int
main(int argc, char *argv[])
{
	/* threshold, Geiger dead time */
	const struct parameters params = {500, 0.0005};

	SF_INFO sinfo;
	SNDFILE* stream = NULL;
	{
		char *filename;
		if (argc < 2)
			filename = "-";
		else
			filename = argv[1];

		stream = openaudiostream(filename, &sinfo);
		assert(stream != NULL);
	}

	struct detectordata *data;
	data = init_detector(sinfo.samplerate, &params, &displaycallback);
	if (data == NULL) {
		fprintf(stderr, "Detector initialization failed\n");
		closeaudiostream(stream);
		return EXIT_FAILURE;
	}

	const size_t spl_size = 128;
	int16_t buffer[spl_size];

	while(1) {
		int nbfr = sf_readf_short(stream, buffer, spl_size);
		if (!nbfr)
			break;
		detector(buffer, nbfr, data);
	}

	closeaudiostream(stream);

	return EXIT_SUCCESS;
}
