
#include "common.h"

static const char CHAIN_NAME_FMT[] = "chain%i";

/**
 * write the stats of each chain to a tstats csv file in
 * tstats_profile_path
 */
int common_write_stats(ubx_block_t *b, struct ubx_chain *chains, int num_chains)
{
	int len;
	const char *profile_path;
	FILE* fp;

	len = cfg_getptr_char(b, "tstats_profile_path", &profile_path);

	if (len < 0) {
		ubx_err(b, "unable to retrieve config tstats_profile_path");
		return -1;
	}

	if (len == 0)
		return 0; 		/* no path, no stat files */


	fp = ubx_tstats_fopen(b, profile_path);

	if (fp == NULL)
		return -1;

	for (int i = 0; i < num_chains; i++) {
		if (ubx_chain_tstats_fwrite(b, fp, &chains[i]) != 0) {
			fclose(fp);
			return -1;
		}
	}

	if (fclose(fp) != 0) {
		ubx_err(b, "failed to close tstats file %s: %m", profile_path);
		return -1;
	}

	return 0;
}

/**
 * log all the stats
 */
void common_log_stats(ubx_block_t *b, struct ubx_chain *chains, int num_chains)
{
	for (int i = 0; i < num_chains; i++)
		ubx_chain_tstats_log(b, &chains[i]);
}

/**
 * output all stats on the tstats port
 */
void common_output_stats(ubx_block_t *b, struct ubx_chain *chains, int num_chains)
{
	for (int i = 0; i < num_chains; i++)
		ubx_chain_tstats_output(b, &chains[i]);
}

/**
 * common_init_chains
 *
 * read the config "num_chains" and create that number of "chainN"
 * configs. allocate memory for num_chains ubx_chain data structures
 * and assign to *chain. To be run in init hook
 */
int common_init_chains(ubx_block_t *b, struct ubx_chain **chain)
{
	int len, ret = -1;
	const int *tint;
	int num_chains;
	char chain_id[UBX_BLOCK_NAME_MAXLEN+1];

	/* num_chains */
	len = cfg_getptr_int(b, "num_chains", &tint);
	assert(len >= 0);
	num_chains = (len > 0) ? *tint : 1;

	if (num_chains < 1) {
		ubx_err(b, "EINVALID_CONFIG: num_chains must be >= 1 but is %u", num_chains);
		goto out_err;
	}

	/* allocate ubx_chain array */
	*chain = realloc(*chain, num_chains * sizeof(struct ubx_chain));

	if (*chain == NULL) {
		ubx_err(b, "EOUTOFMEM allocating struct ubx_chain[%u] array", num_chains);
		goto out_err;
	}
	memset(*chain, 0, num_chains * sizeof(struct ubx_chain));

	/* add the configs to the block and init the ubx_chain's configs */
	for (int i = 0; i<num_chains; i++) {
		snprintf(chain_id, UBX_BLOCK_NAME_MAXLEN, CHAIN_NAME_FMT, i);

		ret = ubx_config_add(b, chain_id, NULL, "struct ubx_triggee");

		if (ret)
			goto out_err;
	}

	return num_chains;

out_err:
	common_cleanup(b, chain, 0);
	return ret;

}

/**
 * common_config_chains
 *
 * retrieve the relevant tstat_ configs and configure all num_chains
 * struct ubx_chain data structures. To be run in start hook.
 *
 * @b: block from which to retrieve configs
 * @chain: pointer to array of ubx_chain structs to configure
 * @num_chains: the number of trigger chains to configure
 * @return 0 if OK, < 0 otherwise
 */
int common_config_chains(const ubx_block_t *b, struct ubx_chain *chain, int num_chains)

{
	int i, len;
	const int *tint;
	const double *tdbl;

	int tstats_mode, tstats_skip_first;
	double output_rate;

	ubx_port_t *p_tstats;
	char chain_id[UBX_BLOCK_NAME_MAXLEN+1];

	/* tstats_mode */
	len = cfg_getptr_int(b, "tstats_mode", &tint);
	assert(len >= 0);
	tstats_mode = (len > 0) ? *tint : 0;

	/* tstats_output_rate */
	len = cfg_getptr_double(b, "tstats_output_rate", &tdbl);
	assert(len >= 0);
	output_rate = (len > 0) ? *tdbl : 0;

	/* tstats_skip_first */
	len = cfg_getptr_int(b, "tstats_skip_first", &tint);
	assert(len >= 0);
	tstats_skip_first = (len > 0) ? *tint : 0;

	/* tstats port */
	p_tstats = ubx_port_get(b, "tstats");
	assert(p_tstats);

	/* initialize all chains */
	for (i = 0; i < num_chains; i++) {
		chain[i].tstats_mode = tstats_mode;
		chain[i].tstats_skip_first = tstats_skip_first;
		chain[i].p_tstats = p_tstats;

		snprintf(chain_id, UBX_BLOCK_NAME_MAXLEN, CHAIN_NAME_FMT, i);

		chain[i].triggees_len =
			cfg_getptr_triggee(b, chain_id, &chain[i].triggees);

		assert(chain[i].triggees_len >= 0);

		if (ubx_chain_init(&chain[i], chain_id, output_rate) != 0)
			goto out_fail;
	}
	return 0;
out_fail:
	ubx_err(b, "failed to configure chain%i", i);

	while (--i >= 0)
		ubx_chain_cleanup(&chain[i]);
	return -1;
}

/**
 * common_read_actchain - read and validate the active chain value
 */
void common_read_actchain(const ubx_block_t *b,
			  const ubx_port_t *p_actchain,
			  const int num_chains,
			  int *actchain)
{
	int ret, tmp;

	ret = read_int(p_actchain, &tmp);

	if (ret == 0) {
		/* no chain switch */
		return;
	} else if ( ret <= 0) {
		ubx_err(b, "reading port %s failed", p_actchain->name);
		return;
	}

	if (tmp < 0 || tmp >= num_chains) {
		ubx_err(b, "requested chain %i is out of range [0,%i]", tmp, num_chains - 1);
		return;
	}

	if (*actchain != tmp)
		ubx_info(b, "active_chain switched to chain%i", tmp);

	*actchain = tmp;
}

/**
 * common_sync_init - cache the sync ports and forget the last served
 * value. To be run in the start hook.
 */
void common_sync_init(const ubx_block_t *b, struct trig_sync *sync)
{
	sync->p_req = ubx_port_get(b, "sync_req");
	sync->p_ack = ubx_port_get(b, "sync_ack");
	assert(sync->p_req != NULL && sync->p_ack != NULL);
	sync->last = 0;
}

/**
 * common_sync_trigger - step the chain once per new sync_req value
 *
 * Values that are not greater than the last served one are skipped,
 * as are values overtaken before this was called (no catch-up).
 *
 * @return 1 if the chain was stepped, 0 if skipped, < 0 on error
 */
int common_sync_trigger(struct trig_sync *sync, struct ubx_chain *chain)
{
	uint64_t req;

	if (read_uint64(sync->p_req, &req) <= 0 || req <= sync->last)
		return 0;

	/* a failed step is not retried: that would be an error per pass
	 * in free-run. Nor is it acked, the peer must not count on it */
	sync->last = req;

	if (ubx_chain_trigger(chain) != 0)
		return -1;

	if (__atomic_load_n(&sync->p_ack->out_interaction, __ATOMIC_RELAXED) != NULL)
		write_uint64(sync->p_ack, &req);

	return 1;
}

/* undo common_config (for stop hook) */
void common_unconfig(struct ubx_chain *chains, int num_chains)
{
	for (int i = 0; i < num_chains; i++)
		ubx_chain_cleanup(&chains[i]);
}

/* undo common_init (for cleanup hook) */
void common_cleanup(ubx_block_t *b, struct ubx_chain **chains, int num_chains)
{
	ubx_config_t *c = NULL, *ctmp = NULL;

	/* remove all dynamically added configs */
	DL_FOREACH_SAFE(b->configs, c, ctmp) {
		if (cfg_is_dyn(c))
			ubx_config_rm(b, c->name);
	}

	if (*chains != NULL) {
		for (int i = 0; i < num_chains; i++)
			ubx_chain_cleanup(&(*chains)[i]);
	}

	free(*chains);
	*chains = NULL;
}
