#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <iostream>

#include "SpeakerX.hpp"
#include "EventBuffer.hpp"


#define SPKDEBUG 0
#if SPKDEBUG
#define DEB(x) x
#else
#define DEB(x)
#endif

uint64_t Speaker::generate_fill_buffer(int16_t *buffer, uint64_t num_samples) {

}

uint64_t Speaker::generate_buffer_int(int16_t *buffer, uint64_t num_samples) {
    double fractional_cycle_per_sample = (config->cycle_per_sample - (int)config->cycle_per_sample);

    for (uint64_t i = 0; i < num_samples; i++) {
        double contribution = 0.0f;
        //int j;

        // number of whole cycles in this sample
        uint64_t sample_end_cycle = (cycle_index + (uint64_t)(config->cycle_per_sample));

        DEB(printf("cycle_index: %llu . %f, leftover: %f sample_end_cycle: %llu\n", cycle_index, frac, leftover, sample_end_cycle));

        /** first, 'consume' any cycle portion left over from last iteration. */
        if (leftover > 0.0f) {
            DEB(printf(" -> leftover: %f\n", leftover));
            contribution += leftover * polarity;
            cycle_index ++;
            // finishing up a fractional cycle, do not check for polarity change.
        }

        DEB(printf(" => [1] : cycle_index: %llu, sample_end_cycle: %llu\n", cycle_index, sample_end_cycle));

        // if there is no event prior to sample_end_cycle, skip the loop and just add the whole part
        /* uint64_t event_time;
        event_buffer->peek_oldest(event_time); */
        if (/* events[event_index] */ event_buffer->peek() > sample_end_cycle) {
            contribution += (polarity * (sample_end_cycle - cycle_index));
            cycle_index = sample_end_cycle;
        } else {
            // loop through the whole cycles in this sample.
            while (cycle_index < /* (uint64_t) */(sample_end_cycle)) {
                DEB(printf(" => [2] : cycle_index: %llu\n", cycle_index));
                getCheckPolarity(cycle_index); // check here for change in polarity event
                contribution += polarity;
                cycle_index ++;
            }
        }

        DEB(printf(" => [3] : cycle_index: %llu\n", cycle_index));

        // add fractional part of cycles per sample to the cycle counter
        frac += fractional_cycle_per_sample;
        DEB(printf(" => leftover: %f, frac: %f\n", leftover, frac));
        
        /** do we have more than a whole cycle? if so, process a whole cycle including potential polarity change. */
        if (frac >= 1.0f) { // there was a case where frac exactly == 1.0f and we had a contribution of 24.14, which is wrooooooooong.
            cycle_index ++;
            getCheckPolarity(cycle_index); // check for change in polarity event.
            contribution +=  polarity; 
            frac -= 1.0f;
        }

        /** if there is a fractional part, process it (but do not check for polarity change, because those only occur on whole cycle boundaries) */
        if (frac > 0.0f) {
            contribution += frac * polarity;
            //cycle_index += frac;
        }
        leftover = 1 - frac;
        DEB(printf("CPS: %f, sample: %llu, cycle_index: %llu, contribution: %f\n", config.cycle_per_sample, i, cycle_index, contribution));
        DEB(printf("Sample: %f\n", contribution / config.cycle_per_sample));
        buffer[i] = (int16_t)((contribution / config->cycle_per_sample) * 5120);
        if (contribution < -config->contribution_max || contribution > config->contribution_max) {
            printf("contr %f > max %f\n", contribution, config->contribution_max);
        }
        polarity *= 0.9999f; // this was in the original code. it may affect the audio quality.
    }
    sample_index += num_samples;
    return num_samples;
}