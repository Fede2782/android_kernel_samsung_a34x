// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include "tpd1.h"

/* #ifdef tpd1_HAVE_BUTTON */
/* static int tpd1_keys[tpd1_KEY_COUNT] = tpd1_KEYS; */
/* static int tpd1_keys_dim[tpd1_KEY_COUNT][4] = tpd1_KEYS_DIM; */
static unsigned int tpd1_keycnt;
static int tpd1_keys[tpd1_VIRTUAL_KEY_MAX] = { 0 };

static int tpd1_keys_dim[tpd1_VIRTUAL_KEY_MAX][4];	/* = {0}; */
/*
static ssize_t mtk_virtual_keys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	int i, j;

	for (i = 0, j = 0; i < tpd1_keycnt; i++)
		j += snprintf(buf+j, PAGE_SIZE-j, "%s%s:%d:%d:%d:%d:%d%s", buf,
			     __stringify(EV_KEY), tpd1_keys[i],
			     tpd1_keys_dim[i][0], tpd1_keys_dim[i][1],
			     tpd1_keys_dim[i][2], tpd1_keys_dim[i][3],
			     (i == tpd1_keycnt - 1 ? "\n" : ":"));
	return j;
}

static struct kobj_attribute mtk_virtual_keys_attr = {
	.attr = {
		 .name = "virtualkeys.mtk-tpd1",
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

void tpd1_button_init(void)
{
	//int ret = 0, i = 0;
	int i = 0;
/* if((tpd1->kpd=input_allocate_device())==NULL) return -ENOMEM; */
	tpd1->kpd = input_allocate_device();
	/* struct input_dev kpd initialization and registration */
	tpd1->kpd->name = tpd1_DEVICE "-kpd";
	set_bit(EV_KEY, tpd1->kpd->evbit);
	/* set_bit(EV_REL, tpd1->kpd->evbit); */
	/* set_bit(EV_ABS, tpd1->kpd->evbit); */
	for (i = 0; i < tpd1_keycnt; i++)
		__set_bit(tpd1_keys[i], tpd1->kpd->keybit);
	tpd1->kpd->id.bustype = BUS_HOST;
	tpd1->kpd->id.vendor = 0x0001;
	tpd1->kpd->id.product = 0x0001;
	tpd1->kpd->id.version = 0x0100;
	if (input_register_device(tpd1->kpd))
		tpd1_DMESG("input_register_device failed.(kpd)\n");
	set_bit(EV_KEY, tpd1->dev->evbit);
	for (i = 0; i < tpd1_keycnt; i++)
		__set_bit(tpd1_keys[i], tpd1->dev->keybit);
	/*properties_kobj = kobject_create_and_add("board_properties", NULL);
	if (properties_kobj)
		ret = sysfs_create_group(properties_kobj,
				&mtk_properties_attr_group);
	if (!properties_kobj || ret)
		tpd1_DEBUG("failed to create board_properties\n");*///hc_rm
}

void tpd1_button(unsigned int x, unsigned int y, unsigned int down)
{
	int i;
	bool report;

	if (down) {
		for (i = 0; i < tpd1_keycnt; i++) {
			report = x >= tpd1_keys_dim[i][0] -
					(tpd1_keys_dim[i][2] / 2) &&
				x <= tpd1_keys_dim[i][0] +
					(tpd1_keys_dim[i][2] / 2) &&
				y >= tpd1_keys_dim[i][1] -
					(tpd1_keys_dim[i][3] / 2) &&
				y <= tpd1_keys_dim[i][1] +
					(tpd1_keys_dim[i][3] / 2) &&
				!(tpd1->btn_state & (1 << i));

			if (report) {
				input_report_key(tpd1->kpd, tpd1_keys[i], 1);
				input_sync(tpd1->kpd);
				tpd1->btn_state |= (1 << i);
				tpd1_DEBUG("press key %d (%d)\n",
						i, tpd1_keys[i]);
			}
		}
	} else {
		for (i = 0; i < tpd1_keycnt; i++) {
			if (tpd1->btn_state & (1 << i)) {
				input_report_key(tpd1->kpd, tpd1_keys[i], 0);
				input_sync(tpd1->kpd);
				tpd1_DEBUG("release key %d (%d)\n",
						i, tpd1_keys[i]);
			}
		}
		tpd1->btn_state = 0;
	}
}

void tpd1_button_setting(int keycnt, void *keys, void *keys_dim)
{
	tpd1_keycnt = keycnt;
	memcpy(tpd1_keys, keys, keycnt * 4);
	memcpy(tpd1_keys_dim, keys_dim, keycnt * 4 * 4);
}

/* #endif */
void tpd1_button_setting_ti941(int keycnt, void *keys)
{
	tpd1_keycnt = keycnt;
	memcpy(tpd1_keys, keys, keycnt * 4);

}