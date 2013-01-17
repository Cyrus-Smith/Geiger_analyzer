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
 * More specifically, it should be used to adapt the input stream into
 * a form easier to process afterward. Here it what this program does
 *  1) make all the data positive, i.e.: data[i] -> abs(data[i])
 *  2) Given the Geiger dead time Tg, for each time interval of size Tg/2
 *    take the max of data[i] on this interval.
 *  3) output the stream of these maxima as the output stream.
 *
 * Use it for instance with SoX [1] and a detector program:
 *    $ sox -d -t wav -c 1 - | streamfilter | peakdetector
 * to analyse the audio stream from the soundcard.
 *
 * [1]: SoX: http://sox.sourceforge.net/
 *
 *
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
openaudiostream(const char* filename, SF_INFO *sinfo, bool in)
{
	assert(filename != NULL);
	assert(sinfo != NULL);
	SNDFILE* stream = NULL;
	if (in)
		stream = sf_open(filename, SFM_READ, sinfo);
	else
		stream = sf_open(filename, SFM_WRITE, sinfo);
	if (stream == NULL) {
		fprintf(stderr, "Unable to open audio stream (%s): %s\n",
			in ? "read" : "write", sf_strerror(NULL));
		exit(EXIT_FAILURE);
	}
	if (!in)
		return stream;

	if (sinfo->channels != 1) {
		fprintf(stderr, "Don't know how to process stream with %d "
			"channels\n", sinfo->channels);
		closeaudiostream(stream);
		exit(EXIT_FAILURE);

	}
	return stream;
}

struct intdata {
	int16_t cmax;
	int16_t cmin;
	int16_t nbpoints;
};

uint32_t totala = 0;
uint32_t totalb = 0;


static uint32_t
process(uint32_t samplerate, double Tg,
	int16_t *inbuffer, size_t inbuffersize,
	int16_t *outbuffer, size_t outbuffersize, struct intdata *inprog)
{
	bool unfinishedbusiness = false;
	const double Tg2 = Tg/2;

	const uint32_t nb_points_inter = Tg2 * samplerate;
	assert(nb_points_inter > 0);

//	fprintf(stderr, "nb_points_inter: %u\n", nb_points_inter);

	assert(inprog->nbpoints < nb_points_inter);

	const uint32_t extra_points = inprog->nbpoints;
	const uint32_t nb_points = inbuffersize + extra_points;
	uint32_t nb_inter = nb_points / nb_points_inter;

	assert(nb_inter > 0);
	assert(nb_points >= nb_inter * nb_points_inter);

	assert(nb_inter <= outbuffersize);

	uint32_t nb_points_last = nb_points - nb_inter * nb_points_inter;
	if (nb_points_last) {
		nb_inter++;
		unfinishedbusiness = true;
	}



	memset(outbuffer, 0, outbuffersize);
	const uint32_t shift = inprog->nbpoints;
	for (unsigned int j = 0; j < nb_inter; j++) { // for each interval
		assert(j < outbuffersize);
		int16_t cmax = 0;
		unsigned int bmin = 0;
		if (!j) {
			bmin = inprog->nbpoints;
			cmax = inprog->cmax;
		}
		unsigned int bmax = nb_points_inter;
		if (j == nb_inter - 1 && unfinishedbusiness) {
			bmax = nb_points_last;
		}
		for (unsigned int i = bmin; i < bmax; i++) {
			// for each point in interval
			totalb++;
			const uint32_t inidx = j * nb_points_inter + i - shift;
			assert(inidx < inbuffersize);
			const int16_t inval = abs(inbuffer[inidx]);
			if (inval > cmax) {
				cmax = inval;
			}
		}
		if (j == nb_inter - 1 && unfinishedbusiness) {
			inprog->cmax = cmax;
			inprog->nbpoints = nb_points_last;
		} else {
			outbuffer[j] = cmax;
		}
	}
	if (unfinishedbusiness) {
		assert((nb_inter - 1) * nb_points_inter
		       == inbuffersize + extra_points - inprog->nbpoints);
		return nb_inter - 1;
	}
	assert(nb_inter * nb_points_inter == inbuffersize + extra_points);
	return nb_inter;
}

static void
usage(void)
{
	fprintf(stderr, "usage: streamfilter Geiger_dead_time_in_seconds [inputfile [outputfile]]\n");
}

int
main(int argc, char *argv[])
{

	double Tg = 0;

	if (argc < 2) {
		usage();
		exit(EXIT_FAILURE);
	}
	Tg = strtod(argv[1], NULL);
	if (Tg * 10000 <= 0 || Tg > 1) {
		fprintf(stderr, "incorrect time specification\n");
		usage();
		exit(EXIT_FAILURE);
	} else {
		fprintf(stderr, "Geiger dead time set to %lf s\n", Tg);
	}

	SF_INFO insinfo;
	SNDFILE* instream = NULL;
	{
		char *filename;
		if (argc < 3)
			filename = "-";
		else
			filename = argv[2];

		memset(&insinfo, 0, sizeof insinfo);
		instream = openaudiostream(filename, &insinfo, true);
		assert(instream != NULL);
	}

	SF_INFO outsinfo;
	SNDFILE* outstream = NULL;
	{
		char *filename;
		if (argc < 3)
			filename = "-";
		else
			filename = argv[3];

		memset(&outsinfo, 0, sizeof outsinfo);
		uint32_t tmp = Tg/2 * insinfo.samplerate;
		outsinfo.samplerate = insinfo.samplerate / tmp;
		fprintf(stderr, "samplerate: %d\n", outsinfo.samplerate);
		outsinfo.channels = 1;
		outsinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

		outstream = openaudiostream(filename, &outsinfo, false);
		assert(outstream != NULL);
	}

	const size_t inspl_size = 128;
	int16_t inbuffer[inspl_size];

	const size_t outbufsize = 128;
	int16_t outbuffer[outbufsize];

	struct intdata tmpdat = {0, 0};

	while(1) {
		int nbfr = sf_readf_short(instream, inbuffer, inspl_size);
		totala += nbfr;
		if (!nbfr)
			break;

		uint32_t actualoutbufsize = process(insinfo.samplerate, Tg,
						    inbuffer, nbfr,
						    outbuffer, outbufsize,
						    &tmpdat);

		sf_writef_short(outstream, outbuffer, actualoutbufsize);
	}

	fprintf(stderr, "total pts: a: %u, b: %u\n", totala, totalb);

	closeaudiostream(instream);
	closeaudiostream(outstream);

	return EXIT_SUCCESS;
}
