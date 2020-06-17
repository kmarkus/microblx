#ifndef UBX_TRIGGEE_H
#define UBX_TRIGGEE_H

/**
 * struct ubx_triggee
 *
 * this datastructure describes a block and how it shall be triggered
 *
 * @b: pointer to block to trigger
 *
 * @num_steps: number of times to trigger the given block (0 and 1
 * 	       mean 1, -1 to disable)
 *
 */
struct ubx_triggee {
	ubx_block_t *b;
	int num_steps;
};

#endif /* UBX_TRIGGEE_H */
