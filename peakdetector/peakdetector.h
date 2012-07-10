#ifndef _PEAKDETECTOR_H_
#define _PEAKDETECTOR_H_

struct parameters {
	unsigned int noise_threshold;  // Detection threshold to filter noise.
	double geiger_dead_time;       // Geiger dead time (in seconds).
};

struct detectordata;

struct detector {
	char *name;
	int (*detector)(int16_t *sample, size_t sample_size, struct detectordata* data);
	int (*terminate)(struct detector* detector);
	struct detectordata *data;
};

#endif /* !_PEAKDETECTOR_H_ */
