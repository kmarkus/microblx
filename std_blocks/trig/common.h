#include "ubx.h"
#include "trig_utils.h"

/**
 * struct trig_sync - state of the sync_req/sync_ack ports
 * @p_req: sync_req in-port
 * @p_ack: sync_ack out-port
 * @last: last served sync_req value, 0 after start
 */
struct trig_sync {
	const ubx_port_t *p_req;
	const ubx_port_t *p_ack;
	uint64_t last;
};

/* sync_req/sync_ack port protos, shared by trig and ptrig */
#define TRIG_SYNC_PORTS							\
	{ .name = "sync_req", .in_type_name = "uint64_t",		\
	  .doc = "if connected: step the active chain once per new (increasing) value, else skip the step" }, \
	{ .name = "sync_ack", .out_type_name = "uint64_t",		\
	  .doc = "the sync_req value served by the last chain step" }

int common_write_stats(ubx_block_t *b, struct ubx_chain *chain, int num_chains);
void common_log_stats(ubx_block_t *b, struct ubx_chain *chains, int num_chains);
void common_output_stats(ubx_block_t *b, struct ubx_chain *chains, int num_chains);

int common_init_chains(ubx_block_t *b, struct ubx_chain **chain);
int common_config_chains(const ubx_block_t *b, struct ubx_chain *chain, int num_chains);
void common_read_actchain(const ubx_block_t *b,
			  const ubx_port_t *p_actchain,
			  const int num_chains,
			  int *actchain);

void common_sync_init(const ubx_block_t *b, struct trig_sync *sync);
int common_sync_trigger(struct trig_sync *sync, struct ubx_chain *chain);

/**
 * common_trigger - trigger a chain, gated by sync_req if connected
 *
 * Unconnected sync_req costs one pointer load on top of
 * ubx_chain_trigger. The load is not dereferenced, so no rcu read
 * section is needed.
 *
 * @return 1 if the chain was stepped, 0 if skipped, < 0 on error
 */
static inline int common_trigger(struct trig_sync *sync, struct ubx_chain *chain)
{
	if (__atomic_load_n(&sync->p_req->in_interaction, __ATOMIC_RELAXED) == NULL)
		return (ubx_chain_trigger(chain) == 0) ? 1 : -1;

	return common_sync_trigger(sync, chain);
}

void common_unconfig(struct ubx_chain *chains, int num_chains);
void common_cleanup(ubx_block_t *b, struct ubx_chain **chain, int num_chains);
