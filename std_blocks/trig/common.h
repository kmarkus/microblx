#include "ubx.h"
#include "trig_utils.h"

int common_write_stats(ubx_block_t *b, struct ubx_chain *chain, int num_chains);
void common_log_stats(ubx_block_t *b, struct ubx_chain *chains, int num_chains);
void common_output_stats(ubx_block_t *b, struct ubx_chain *chains, int num_chains);

int common_init_chains(ubx_block_t *b, struct ubx_chain **chain);
int common_config_chains(const ubx_block_t *b, struct ubx_chain *chain, int num_chains);
void common_read_actchain(const ubx_block_t *b,
			  const ubx_port_t *p_actchain,
			  const int num_chains,
			  int *actchain);

void common_unconfig(struct ubx_chain *chains, int num_chains);
void common_cleanup(ubx_block_t *b, struct ubx_chain **chain);
