// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "tpd2.h"

/* #ifdef tpd2_HAVE_BUTTON */
/* static int tpd2_keys[tpd2_KEY_COUNT] = tpd2_KEYS; */
/* static int tpd2_keys_dim[tpd2_KEY_COUNT][4] = tpd2_KEYS_DIM; */
static unsigned int tpd2_keycnt;
static int tpd2_keys[tpd2_VIRTUAL_KEY_MAX] = { 0 };

static int tpd2_keys_dim[tpd2_VIRTUAL_KEY_MAX][4];	/* = {0}; */
/*
static ssize_t mtk_virtual_keys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	int i, j;

	for (i = 0, j = 0; i < tpd2_keycnt; i++)
		j += snprintf(buf+j, PAGE_SIZE-j, "%s%s:%d:%d:%d:%d:%d%s", buf,
			     __stringify(EV_KEY), tpd2_keys[i],
			     tpd2_keys_dim[i][0], tpd2_keys_dim[i][1],
			     tpd2_keys_dim[i][2], tpd2_keys_dim[i][3],
			     (i == tpd2_keycnt - 1 ? "\n" : ":"));
	return j;
}

static struct kobj_attribute mtk_virtual_keys_attr = {
	.attr = {
		 .name = "virtualkeys.mtk-tpd2",
		 .mode = 0644,
		 },
	.show = &mtk_virtual_keys_show,
};

static struct attribute *mtk_properties_attrs[] = {
	&mtk_virtual_keys_attr.attr,
	NULL
};

static struct attribute_group mtk_properties_attr_group = {
	.attrs = mtk_properties_attrs,
};
*///hc_rm
struct kobject *properties_kobj;

void tpd2_button_init(void)
{
	//int ret = 0, i = 0;
	int i = 0;
/* if((tpd2->kpd=input_allocate_device())==NULL) return -ENOMEM; */
	tpd2->kpd = input_allocate_device();
	/* struct input_dev kpd initialization and registration */
	tpd2->kpd->name = tpd2_DEVICE "-kpd";
	set_bit(EV_KEY, tpd2->kpd->evbit);
	/* set_bit(EV_REL, tpd2->kpd->evbit); */
	/* set_bit(EV_ABS, tpd2->kpd->evbit); */
	for (i = 0; i < tpd2_keycnt; i++)
		__set_bit(tpd2_keys[i], tpd2->kpd->keybit);
	tpd2->kpd->id.bustype = BUS_HOST;
	tpd2->kpd->id.vendor = 0x0001;
	tpd2->kpd->id.product = 0x0001;
	tpd2->kpd->id.version = 0x0100;
	if (input_register_device(tpd2->kpd))
		tpd2_DMESG("input_register_device failed.(kpd)\n");
	set_bit(EV_KEY, tpd2->dev->evbit);
	for (i = 0; i < tpd2_keycnt; i++)
		__set_bit(tpd2_keys[i], tpd2->dev->keybit);
	/*properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&mtk_properties_attr_group);
	if (!properties_kobj || ret)
		tpd2_DEBUG("failed to create board_properties\n");*///hc_rm
}

void tpd2_button(unsigned int x, unsigned int y, unsigned int down)
{
	int i;
	bool report;

	if (down) {
		for (i = 0; i < tpd2_keycnt; i++) {
			report = x >= tpd2_keys_dim[i][0] -
					(tpd2_keys_dim[i][2] / 2) &&
				x <= tpd2_keys_dim[i][0] +
					(tpd2_keys_dim[i][2] / 2) &&
				y >= tpd2_keys_dim[i][1] -
					(tpd2_keys_dim[i][3] / 2) &&
				y <= tpd2_keys_dim[i][1] +
					(tpd2_keys_dim[i][3] / 2) &&
				!(tpd2->btn_state & (1 << i));

			if (report) {
				input_report_key(tpd2->kpd, tpd2_keys[i], 1);
				input_sync(tpd2->kpd);
				tpd2->btn_state |= (1 << i);
				tpd2_DEBUG("press key %d (%d)\n",
						i, tpd2_keys[i]);
			}
		}
	} else {
		for (i = 0; i < tpd2_keycnt; i++) {
			if (tpd2->btn_state & (1 << i)) {
				input_report_key(tpd2->kpd, tpd2_keys[i], 0);
				input_sync(tpd2->kpd);
				tpd2_DEBUG("release key %d (%d)\n",
						i, tpd2_keys[i]);
			}
		}
		tpd2->btn_state = 0;
	}
}

void tpd2_button_setting(int keycnt, void *keys, void *keys_dim)
{
	tpd2_keycnt = keycnt;
	memcpy(tpd2_keys, keys, keycnt * 4);
	memcpy(tpd2_keys_dim, keys_dim, keycnt * 4 * 4);
}

/* #endif */
void tpd2_button_setting_ti941(int keycnt, void *keys)
{
	tpd2_keycnt = keycnt;
	memcpy(tpd2_keys, keys, keycnt * 4);

}