/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include <ctype.h>

#include <rte_telemetry.h>

#include <roc_api.h>

#include "cnxk_ethdev.h"

#define STR_MAXLEN 20
#define W0_MAXLEN  21

static int
copy_outb_sa_9k(struct rte_tel_data *d, uint32_t i, void *sa)
{
	union {
		struct roc_ie_on_sa_ctl ctl;
		uint64_t u64;
	} w0;
	struct roc_ie_on_outb_sa *out_sa;
	char strw0[W0_MAXLEN];
	char str[STR_MAXLEN];

	out_sa = (struct roc_ie_on_outb_sa *)sa;
	w0.ctl = out_sa->common_sa.ctl;

	snprintf(str, sizeof(str), "outsa_w0_%u", i);
	snprintf(strw0, sizeof(strw0), "%" PRIu64, w0.u64);
	rte_tel_data_add_dict_string(d, str, strw0);

	return 0;
}

static int
copy_inb_sa_9k(struct rte_tel_data *d, uint32_t i, void *sa)
{
	union {
		struct roc_ie_on_sa_ctl ctl;
		uint64_t u64;
	} w0;
	struct roc_ie_on_inb_sa *in_sa;
	char strw0[W0_MAXLEN];
	char str[STR_MAXLEN];

	in_sa = (struct roc_ie_on_inb_sa *)sa;
	w0.ctl = in_sa->common_sa.ctl;

	snprintf(str, sizeof(str), "insa_w0_%u", i);
	snprintf(strw0, sizeof(strw0), "%" PRIu64, w0.u64);
	rte_tel_data_add_dict_string(d, str, strw0);

	snprintf(str, sizeof(str), "insa_esnh_%u", i);
	rte_tel_data_add_dict_uint(d, str, in_sa->common_sa.seq_t.th);

	snprintf(str, sizeof(str), "insa_esnl_%u", i);
	rte_tel_data_add_dict_uint(d, str, in_sa->common_sa.seq_t.tl);

	return 0;
}

static int
copy_outb_sa_10k(struct rte_tel_data *d, uint32_t i, void *sa)
{
	struct roc_ot_ipsec_outb_sa *out_sa;
	struct rte_tel_data *outer_hdr;
	char str[STR_MAXLEN];
	char s64[W0_MAXLEN];
	uint32_t j;

	out_sa = (struct roc_ot_ipsec_outb_sa *)sa;

	snprintf(str, sizeof(str), "outsa_w0_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->w0.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_w1_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->w1.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_w2_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->w2.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_w10_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->w10.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	outer_hdr = rte_tel_data_alloc();
	if (!outer_hdr) {
		plt_err("Could not allocate space for outer header");
		return -ENOMEM;
	}

	rte_tel_data_start_array(outer_hdr, RTE_TEL_UINT_VAL);

	for (j = 0; j < RTE_DIM(out_sa->outer_hdr.ipv6.src_addr); j++)
		rte_tel_data_add_array_uint(outer_hdr,
					    out_sa->outer_hdr.ipv6.src_addr[j]);

	for (j = 0; j < RTE_DIM(out_sa->outer_hdr.ipv6.dst_addr); j++)
		rte_tel_data_add_array_uint(outer_hdr,
					    out_sa->outer_hdr.ipv6.dst_addr[j]);

	snprintf(str, sizeof(str), "outsa_outer_hdr_%u", i);
	rte_tel_data_add_dict_container(d, str, outer_hdr, 0);

	snprintf(str, sizeof(str), "outsa_errctl_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->ctx.err_ctl.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_esnval_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->ctx.esn_val);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_hl_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->ctx.hard_life);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_sl_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->ctx.soft_life);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_octs_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->ctx.mib_octs);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "outsa_pkts_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, out_sa->ctx.mib_pkts);
	rte_tel_data_add_dict_string(d, str, s64);

	return 0;
}

static int
copy_inb_sa_10k(struct rte_tel_data *d, uint32_t i, void *sa)
{
	struct roc_ot_ipsec_inb_sa *in_sa;
	struct rte_tel_data *outer_hdr;
	char str[STR_MAXLEN];
	char s64[W0_MAXLEN];
	uint32_t j;

	in_sa = (struct roc_ot_ipsec_inb_sa *)sa;

	snprintf(str, sizeof(str), "insa_w0_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->w0.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_w1_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->w1.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_w2_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->w2.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_w10_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->w10.u64);
	rte_tel_data_add_dict_string(d, str, s64);

	outer_hdr = rte_tel_data_alloc();
	if (!outer_hdr) {
		plt_err("Could not allocate space for outer header");
		return -ENOMEM;
	}

	rte_tel_data_start_array(outer_hdr, RTE_TEL_UINT_VAL);

	for (j = 0; j < RTE_DIM(in_sa->outer_hdr.ipv6.src_addr); j++)
		rte_tel_data_add_array_uint(outer_hdr,
					    in_sa->outer_hdr.ipv6.src_addr[j]);

	for (j = 0; j < RTE_DIM(in_sa->outer_hdr.ipv6.dst_addr); j++)
		rte_tel_data_add_array_uint(outer_hdr,
					    in_sa->outer_hdr.ipv6.dst_addr[j]);

	snprintf(str, sizeof(str), "insa_outer_hdr_%u", i);
	rte_tel_data_add_dict_container(d, str, outer_hdr, 0);

	snprintf(str, sizeof(str), "insa_arbase_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->ctx.ar_base);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_ar_validm_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->ctx.ar_valid_mask);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_hl_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->ctx.hard_life);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_sl_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->ctx.soft_life);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_octs_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->ctx.mib_octs);
	rte_tel_data_add_dict_string(d, str, s64);

	snprintf(str, sizeof(str), "insa_pkts_%u", i);
	snprintf(s64, sizeof(s64), "%" PRIu64, in_sa->ctx.mib_pkts);
	rte_tel_data_add_dict_string(d, str, s64);

	return 0;
}

/* n_vals is the number of params to be parsed. */
static int
parse_params(const char *params, uint32_t *vals, size_t n_vals)
{
	char dlim[2] = ",";
	char *params_args;
	size_t count = 0;
	char *token;

	if (vals == NULL || params == NULL || strlen(params) == 0)
		return -1;

	/* strtok expects char * and param is const char *. Hence on using
	 * params as "const char *" compiler throws warning.
	 */
	params_args = strdup(params);
	if (params_args == NULL)
		return -1;

	token = strtok(params_args, dlim);
	while (token && isdigit(*token) && count < n_vals) {
		vals[count++] = strtoul(token, NULL, 10);
		token = strtok(NULL, dlim);
	}

	free(params_args);

	if (count < n_vals)
		return -1;

	return 0;
}

static int
ethdev_sec_tel_handle_sa_info(const char *cmd __rte_unused, const char *params,
			      struct rte_tel_data *d)
{
	struct cnxk_eth_sec_sess *eth_sec, *tvar;
	struct rte_eth_dev *eth_dev;
	struct cnxk_eth_dev *dev;
	uint32_t port_id, sa_idx;
	uint32_t vals[2] = {0};
	uint32_t i;
	int ret;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	if (parse_params(params, vals, RTE_DIM(vals)) < 0)
		return -EINVAL;

	port_id = vals[0];
	sa_idx = vals[1];

	if (!rte_eth_dev_is_valid_port(port_id)) {
		plt_err("Invalid port id %u", port_id);
		return -EINVAL;
	}

	eth_dev = &rte_eth_devices[port_id];
	if (!eth_dev) {
		plt_err("Ethdev not available");
		return -EINVAL;
	}
	dev = cnxk_eth_pmd_priv(eth_dev);

	rte_tel_data_start_dict(d);

	i = 0;
	if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY) {
		tvar = NULL;
		RTE_TAILQ_FOREACH_SAFE(eth_sec, &dev->outb.list, entry, tvar) {
			if (eth_sec->sa_idx == sa_idx) {
				rte_tel_data_add_dict_int(d, "outb_sa", 1);
				if (roc_model_is_cn10k())
					ret = copy_outb_sa_10k(d, i, eth_sec->sa);
				else
					ret = copy_outb_sa_9k(d, i, eth_sec->sa);
				if (ret < 0)
					return ret;
				break;
			}
		}
	}

	i = 0;
	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		tvar = NULL;
		RTE_TAILQ_FOREACH_SAFE(eth_sec, &dev->inb.list, entry, tvar) {
			if (eth_sec->sa_idx == sa_idx) {
				rte_tel_data_add_dict_int(d, "inb_sa", 1);
				if (roc_model_is_cn10k())
					ret = copy_inb_sa_10k(d, i, eth_sec->sa);
				else
					ret = copy_inb_sa_9k(d, i, eth_sec->sa);
				if (ret < 0)
					return ret;
				break;
			}
		}
	}
	return 0;
}

static int
ethdev_sec_tel_handle_info(const char *cmd __rte_unused, const char *params,
			   struct rte_tel_data *d)
{
	uint32_t min_outb_sa = UINT32_MAX, max_outb_sa = 0;
	uint32_t min_inb_sa = UINT32_MAX, max_inb_sa = 0;
	struct cnxk_eth_sec_sess *eth_sec, *tvar;
	struct rte_eth_dev *eth_dev;
	struct cnxk_eth_dev *dev;
	uint16_t port_id;
	char *end_p;

	if (params == NULL || strlen(params) == 0 || !isdigit(*params))
		return -EINVAL;

	port_id = strtoul(params, &end_p, 0);
	if (errno != 0)
		return -EINVAL;

	if (*end_p != '\0')
		plt_err("Extra parameters passed to telemetry, ignoring it");

	if (!rte_eth_dev_is_valid_port(port_id)) {
		plt_err("Invalid port id %u", port_id);
		return -EINVAL;
	}

	eth_dev = &rte_eth_devices[port_id];
	if (!eth_dev) {
		plt_err("Ethdev not available");
		return -EINVAL;
	}

	dev = cnxk_eth_pmd_priv(eth_dev);

	rte_tel_data_start_dict(d);

	rte_tel_data_add_dict_int(d, "nb_outb_sa", dev->outb.nb_sess);

	if (!dev->outb.nb_sess)
		min_outb_sa = 0;

	if (dev->tx_offloads & RTE_ETH_TX_OFFLOAD_SECURITY) {
		tvar = NULL;
		RTE_TAILQ_FOREACH_SAFE(eth_sec, &dev->outb.list, entry, tvar) {
			if (eth_sec->sa_idx < min_outb_sa)
				min_outb_sa = eth_sec->sa_idx;
			if (eth_sec->sa_idx > max_outb_sa)
				max_outb_sa = eth_sec->sa_idx;
		}
		rte_tel_data_add_dict_int(d, "min_outb_sa", min_outb_sa);
		rte_tel_data_add_dict_int(d, "max_outb_sa", max_outb_sa);
	}

	rte_tel_data_add_dict_int(d, "nb_inb_sa", dev->inb.nb_sess);

	if (!dev->inb.nb_sess)
		min_inb_sa = 0;

	if (dev->rx_offloads & RTE_ETH_RX_OFFLOAD_SECURITY) {
		tvar = NULL;
		RTE_TAILQ_FOREACH_SAFE(eth_sec, &dev->inb.list, entry, tvar) {
			if (eth_sec->sa_idx < min_inb_sa)
				min_inb_sa = eth_sec->sa_idx;
			if (eth_sec->sa_idx > max_inb_sa)
				max_inb_sa = eth_sec->sa_idx;
		}
		rte_tel_data_add_dict_int(d, "min_inb_sa", min_inb_sa);
		rte_tel_data_add_dict_int(d, "max_inb_sa", max_inb_sa);
	}

	return 0;
}

RTE_INIT(cnxk_ipsec_init_telemetry)
{
	rte_telemetry_register_cmd("/cnxk/ipsec/info",
				   ethdev_sec_tel_handle_info,
				   "Returns number of SA's and Max and Min SA. Parameters: port id");
	rte_telemetry_register_cmd("/cnxk/ipsec/sa_info",
				   ethdev_sec_tel_handle_sa_info,
				   "Returns ipsec info. Parameters: port id & sa_idx");
}
