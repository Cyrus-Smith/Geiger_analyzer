#ifndef _PEAKDETECTOR_H_
#define _PEAKDETECTOR_H_

struct parameters {
	unsigned int noise_threshold;  // Detection threshold to filter noise.
	double geiger_dead_time;       // Geiger dead time (in seconds).
};

#endif /* !_PEAKDETECTOR_H_ */
