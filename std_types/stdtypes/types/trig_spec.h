#ifndef UBX_TRIG_SPEC_H
#define UBX_TRIG_SPEC_H

/**
 * struct ubx_trig_spec - trigger specification
 *
 * @b: pointer to block to trigger
 * @num_steps: number of times to trigger the given block
 * @measure: boolean value of whether to measure
 *           timing behavior of the respective block
 *
 * This is a generic trigger specification entry for use as
 * configuration of trigger blocks.
 */
struct ubx_trig_spec {
	ubx_block_t *b;
	unsigned int num_steps;
};

#endif /* UBX_TRIG_SPEC_H */
