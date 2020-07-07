
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
		if (ubx_chain_tstats_fwrite(b, fp, chains) != 0)
			return -1;
	}

	fclose(fp);
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
	common_cleanup(b, chain);
	return ret;

}

/**
 * common_config_chains
 *
 * retrieve the relevant tstat_ configs and configure all num_chains
 * struct ubx_chain data structures. To be run in start hook.
 *
 * @b: block from which to retrieve configs
 * @ubx_chain: pointer pointer to ubx_chain. The pointer will be assigned to
 * 	       allocated memory which must be freed using @common_cleanup.
 * @num_chains: the number of trigger lists to create
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
		ubx_err(b, "requested chain %i is out of range [0,%i]", tmp, num_chains);
		return;
	}

	if (*actchain != tmp)
		ubx_info(b, "active_chain switched to chain%i", tmp);

	*actchain = tmp;
}

/* undo common_config (for stop hook) */
void common_unconfig(struct ubx_chain *chains, int num_chains)
{
	for (int i = 0; i < num_chains; i++)
		ubx_chain_cleanup(&chains[i]);
}

/* undo common_init (for cleanup hook) */
void common_cleanup(ubx_block_t *b, struct ubx_chain **chains)
{
	ubx_config_t *c = NULL, *ctmp = NULL;

	ubx_alert(b, "%s", __func__);
	/* remove all dynamically added configs */
	DL_FOREACH_SAFE(b->configs, c, ctmp) {
		if (cfg_is_dyn(c)) {
			ubx_alert(b, "%s: removing config %s", __func__, c->name);
			ubx_config_rm(b, c->name);
		}
	}

	free(*chains);
	*chains = NULL;
}
