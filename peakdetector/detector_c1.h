#ifndef _DETECTOR_C1_H_
#define _DETECTOR_C1_H_

#include <stdint.h>
#include <stdlib.h>

#include "peakdetector.h"

struct detector* init_detector_c1(uint32_t sample_rate, const struct parameters *params, void (*callback)(double, int16_t));

#endif /* !_DETECTOR_C1_H_ */
