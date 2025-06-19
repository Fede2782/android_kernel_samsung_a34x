// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/interconnect-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <dt-bindings/interconnect/mtk,emibus.h>

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
#include "internal.h"
#include <trace/events/mtk_qos_trace.h>
#endif

enum mtk_icc_name {
	SLAVE__EMI_BUS = 0x1000,
	MASTER_MCUSYS,
	MASTER_GPUSYS,
	MASTER_MMSYS,
	MASTER_MM_VPU,
	MASTER_MM_DISP,
	MASTER_MM_VDEC,
	MASTER_MM_VENC,
	MASTER_MM_CAM,
	MASTER_MM_IMG,
	MASTER_MM_MDP,
	MASTER_DBGIF,
};

#define MT6989_MAX_LINKS	1

/**
 * struct mtk_icc_node - Mediatek specific interconnect nodes
 * @name: the node name used in debugfs
 * @ep : the type of this endpoint
 * @id: a unique node identifier
 * @links: an array of nodes where we can go next while traversing
 * @num_links: the total number of @links
 * @buswidth: width of the interconnect between a node and the bus
 * @sum_avg: current sum aggregate value of all avg bw kBps requests
 * @max_peak: current max aggregate value of all peak bw kBps requests
 */
struct mtk_icc_node {
	unsigned char *name;
	int ep;
	u16 id;
	u16 links[MT6989_MAX_LINKS];
	u16 num_links;
	u64 sum_avg;
	u64 max_peak;
};

struct mtk_icc_desc {
	struct mtk_icc_node **nodes;
	size_t num_nodes;
};

#define DEFINE_MNODE(_name, _id, _ep, ...)	\
		static struct mtk_icc_node _name = {			\
		.name = #_name,						\
		.id = _id,						\
		.ep = _ep,						\
		.num_links = ARRAY_SIZE(((int[]){ __VA_ARGS__ })),	\
		.links = { __VA_ARGS__ },				\
}

struct vir_opp {
	u32 require_bw;
	u32 opp_level;
};
struct dvfsrc_emibus_icc_provider {
	struct icc_provider provider;
	struct device *dev;
	struct device *ctr_dev;
	u32 num_opp;
	struct vir_opp *vir_opps;
};

DEFINE_MNODE(emi_bus, SLAVE__EMI_BUS, 1);
DEFINE_MNODE(mcusys, MASTER_MCUSYS, 0, SLAVE__EMI_BUS);
DEFINE_MNODE(gpusys, MASTER_GPUSYS, 0, SLAVE__EMI_BUS);
DEFINE_MNODE(mmsys, MASTER_MMSYS, 0, SLAVE__EMI_BUS);
DEFINE_MNODE(mm_disp, MASTER_MM_DISP, 0, SLAVE__EMI_BUS);
DEFINE_MNODE(mm_vdec, MASTER_MM_VDEC, 0, SLAVE__EMI_BUS);
DEFINE_MNODE(mm_venc, MASTER_MM_VENC, 0, SLAVE__EMI_BUS);
DEFINE_MNODE(mm_cam, MASTER_MM_CAM, 0, SLAVE__EMI_BUS);
DEFINE_MNODE(dbgif, MASTER_DBGIF, 0, SLAVE__EMI_BUS);

static struct mtk_icc_node *mt6989_emibus_icc_nodes[] = {
	[MT6989_SLAVE_EMI_BUS] = &emi_bus,
	[MT6989_MASTER_EB_MCUSYS] = &mcusys,
	[MT6989_MASTER_EB_GPUSYS] = &gpusys,
	[MT6989_MASTER_EB_MMSYS] = &mmsys,
	[MT6989_MASTER_EB_MM_DISP] = &mm_disp,
	[MT6989_MASTER_EB_MM_VDEC] = &mm_vdec,
	[MT6989_MASTER_EB_MM_VENC] = &mm_venc,
	[MT6989_MASTER_EB_MM_CAM] = &mm_cam,
	[MT6989_MASTER_EB_DBGIF] = &dbgif,
};

static struct mtk_icc_desc mt6989_icc_emibus = {
	.nodes = mt6989_emibus_icc_nodes,
	.num_nodes = ARRAY_SIZE(mt6989_emibus_icc_nodes),
};

static const struct of_device_id emibus_icc_of_match[] = {
	{ .compatible = "mediatek-emibus-icc", .data = &mt6989_icc_emibus},
	{ .compatible = "mediatek,mt6989-dvfsrc", .data = &mt6989_icc_emibus},
	{ },
};
MODULE_DEVICE_TABLE(of, emibus_icc_of_match);

static int emi_bus_opp_setting(struct dvfsrc_emibus_icc_provider *emibus_icc_p)
{
	struct device_node *np = emibus_icc_p->dev->of_node;
	struct device_node *opp_np, *opp_node;
	int index = 0, num_opp = 0;
	u32 bw, level;

	opp_np = of_parse_phandle(np, "operating-opp-table", 0);
	if (unlikely(!opp_np))
		return -EINVAL;

	num_opp = of_get_available_child_count(opp_np);
	if (num_opp > 0) {
		emibus_icc_p->vir_opps = devm_kzalloc(emibus_icc_p->dev,
			 num_opp * sizeof(struct vir_opp), GFP_KERNEL);

		if (!emibus_icc_p->vir_opps) {
			of_node_put(opp_np);
			return -ENOMEM;
		}

		for_each_available_child_of_node(opp_np, opp_node) {
			if (of_property_read_u32(opp_node, "opp-bw-KBps", &bw)) {
				of_node_put(opp_node);
				dev_info(emibus_icc_p->dev, "get bw-KBps error\n");
				of_node_put(opp_np);
				return -EINVAL;
			}
			if (of_property_read_u32(opp_node, "level", &level)) {
				of_node_put(opp_node);
				dev_info(emibus_icc_p->dev, "get level error\n");
				of_node_put(opp_np);
				return -EINVAL;
			}
			emibus_icc_p->vir_opps[index].require_bw = bw;
			emibus_icc_p->vir_opps[index].opp_level = level;
			index++;
		}
	} else
		num_opp = 0;

	of_node_put(opp_np);
	emibus_icc_p->num_opp = num_opp;

	return 0;
}

static u32 emi_find_bw_floor_level(struct dvfsrc_emibus_icc_provider *emibus_icc_p, u32 bw)
{
	int i;
	u32 level = 0;

	for (i = 0; i < emibus_icc_p->num_opp; i++) {
		if (bw >= emibus_icc_p->vir_opps[i].require_bw) {
			level = emibus_icc_p->vir_opps[i].opp_level;
			break;
		}
	}

	return level;
}

static int emibus_icc_aggregate(struct icc_node *node, u32 tag, u32 avg_bw,
			     u32 peak_bw, u32 *agg_avg, u32 *agg_peak)
{
	struct mtk_icc_node *in;

	in = node->data;

	*agg_avg += avg_bw;
	*agg_peak = max_t(u32, *agg_peak, peak_bw);

	in->sum_avg = *agg_avg;
	in->max_peak = *agg_peak;

	return 0;
}
#define to_emibus_icc_provider(_provider) \
	container_of(_provider, struct dvfsrc_emibus_icc_provider, provider)

static int emibus_icc_set(struct icc_node *src, struct icc_node *dst)
{
	int ret = 0;
	u32 level = 0;
	struct mtk_icc_node *node;
	struct dvfsrc_emibus_icc_provider *emibus_icc_p;

	node = dst->data;

	if (node->ep == 1) {
		emibus_icc_p = to_emibus_icc_provider(src->provider);
		level = emi_find_bw_floor_level(emibus_icc_p, node->max_peak);
		mtk_dvfsrc_send_request(emibus_icc_p->dev->parent,
			MTK_DVFSRC_CMD_EMICLK_REQUEST, level);

#if IS_ENABLED(CONFIG_MTK_DVFSRC)
		trace_mtk_pm_qos_update_request(0x60, src->peak_bw / 1000, src->name);
#endif
	}

	return ret;
}

static int emibus_icc_get_bw(struct icc_node *node, u32 *avg, u32 *peak)
{
	*avg = 0;
	*peak = 0;
	return 0;
}

static int emibus_icc_probe(struct platform_device *pdev)
{
	const struct mtk_icc_desc *desc;
	struct device *dev = &pdev->dev;
	struct icc_node *node;
	struct icc_onecell_data *data;
	struct icc_provider *provider;
	struct dvfsrc_emibus_icc_provider *emibus_icc_p;
	struct mtk_icc_node **mnodes;
	size_t num_nodes, i, j;
	int ret;

	desc = of_device_get_match_data(dev);
	if (!desc) {
		pr_info("match error\n");
		return -EINVAL;
	}
	if (!dev->parent) {
		pr_info("parent error\n");
		return -EINVAL;
	}
	mnodes = desc->nodes;
	num_nodes = desc->num_nodes;

	emibus_icc_p = devm_kzalloc(dev, sizeof(*emibus_icc_p), GFP_KERNEL);
	if (!emibus_icc_p)
		return -ENOMEM;

	emibus_icc_p->dev = dev;
	ret = emi_bus_opp_setting(emibus_icc_p);
	if (ret)
		return ret;

	data = devm_kzalloc(dev, struct_size(data, nodes, num_nodes),
			GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	provider = &emibus_icc_p->provider;
	provider->dev = dev;
	provider->set = emibus_icc_set;
	provider->aggregate = emibus_icc_aggregate;
	provider->xlate = of_icc_xlate_onecell;
	INIT_LIST_HEAD(&provider->nodes);
	provider->data = data;
	provider->get_bw = emibus_icc_get_bw;

	icc_provider_init(provider);
	for (i = 0; i < num_nodes; i++) {
		node = icc_node_create(mnodes[i]->id);
		if (IS_ERR(node)) {
			ret = PTR_ERR(node);
			goto err;
		}

		node->name = mnodes[i]->name;
		node->data = mnodes[i];
		icc_node_add(node, provider);

		/* populate links */
		for (j = 0; j < mnodes[i]->num_links; j++)
			icc_link_create(node, mnodes[i]->links[j]);

		data->nodes[i] = node;
	}
	data->num_nodes = num_nodes;

	ret = icc_provider_register(provider);
	if (ret)
		goto err;

	platform_set_drvdata(pdev, emibus_icc_p);

	return 0;

err:
	icc_nodes_remove(provider);
	return ret;
}

static int emibus_icc_remove(struct platform_device *pdev)
{
	struct dvfsrc_emibus_icc_provider *emibus_icc_p = platform_get_drvdata(pdev);

	icc_provider_deregister(&emibus_icc_p->provider);
	icc_nodes_remove(&emibus_icc_p->provider);

	return 0;
}

static struct platform_driver emi_icc_emibus_driver = {
	.probe = emibus_icc_probe,
	.remove = emibus_icc_remove,
	.driver = {
		.name = "mediatek-emibus-icc",
		.sync_state = icc_sync_state,
		.of_match_table = emibus_icc_of_match,
	},
};

static int __init mtk_emi_icc_init(void)
{
	return platform_driver_register(&emi_icc_emibus_driver);
}
subsys_initcall(mtk_emi_icc_init);

static void __exit mtk_emi_icc_exit(void)
{
	platform_driver_unregister(&emi_icc_emibus_driver);
}
module_exit(mtk_emi_icc_exit);

MODULE_AUTHOR("Arvin <arvin.wang@mediatek.com>");
MODULE_LICENSE("GPL");
