#ifndef _DETECTOR_H_
#define _DETECTOR_H_

#include <stdint.h>
#include <stdlib.h>

#include "peakdetector.h"

struct detectordata;


struct detectordata* init_detector(uint32_t sample_rate, const struct parameters *params, void (*callback)(double, int16_t));

int detector(int16_t *sample, size_t sample_size, struct detectordata* data);

int terminate_detector(struct detectordata* data);

#endif /* !_DETECTOR_H_ */
