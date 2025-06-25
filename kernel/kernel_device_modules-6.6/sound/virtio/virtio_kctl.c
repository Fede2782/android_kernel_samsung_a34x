// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 MediaTek Inc.
 * virtio-snd: Virtio sound device
 * Copyright (C) 2022 OpenSynergy GmbH
 */
#include <sound/control.h>
#include <linux/virtio_config.h>

#include "virtio_card.h"

/* Map for converting VirtIO types to ALSA types. */
static const snd_ctl_elem_type_t g_v2a_type_map[] = {
	[VIRTIO_SND_CTL_TYPE_BOOLEAN] = SNDRV_CTL_ELEM_TYPE_BOOLEAN,
	[VIRTIO_SND_CTL_TYPE_INTEGER] = SNDRV_CTL_ELEM_TYPE_INTEGER,
	[VIRTIO_SND_CTL_TYPE_INTEGER64] = SNDRV_CTL_ELEM_TYPE_INTEGER64,
	[VIRTIO_SND_CTL_TYPE_ENUMERATED] = SNDRV_CTL_ELEM_TYPE_ENUMERATED,
	[VIRTIO_SND_CTL_TYPE_BYTES] = SNDRV_CTL_ELEM_TYPE_BYTES,
	[VIRTIO_SND_CTL_TYPE_IEC958] = SNDRV_CTL_ELEM_TYPE_IEC958
};

/* Map for converting VirtIO access rights to ALSA access rights. */
static const unsigned int g_v2a_access_map[] = {
	[VIRTIO_SND_CTL_ACCESS_READ] = SNDRV_CTL_ELEM_ACCESS_READ,
	[VIRTIO_SND_CTL_ACCESS_WRITE] = SNDRV_CTL_ELEM_ACCESS_WRITE,
	[VIRTIO_SND_CTL_ACCESS_VOLATILE] = SNDRV_CTL_ELEM_ACCESS_VOLATILE,
	[VIRTIO_SND_CTL_ACCESS_INACTIVE] = SNDRV_CTL_ELEM_ACCESS_INACTIVE,
	[VIRTIO_SND_CTL_ACCESS_TLV_READ] = SNDRV_CTL_ELEM_ACCESS_TLV_READ,
	[VIRTIO_SND_CTL_ACCESS_TLV_WRITE] = SNDRV_CTL_ELEM_ACCESS_TLV_WRITE,
	[VIRTIO_SND_CTL_ACCESS_TLV_COMMAND] = SNDRV_CTL_ELEM_ACCESS_TLV_COMMAND
};

/* Map for converting VirtIO event masks to ALSA event masks. */
static const unsigned int g_v2a_mask_map[] = {
	[VIRTIO_SND_CTL_EVT_MASK_VALUE] = SNDRV_CTL_EVENT_MASK_VALUE,
	[VIRTIO_SND_CTL_EVT_MASK_INFO] = SNDRV_CTL_EVENT_MASK_INFO,
	[VIRTIO_SND_CTL_EVT_MASK_TLV] = SNDRV_CTL_EVENT_MASK_TLV
};
#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
struct virtsnd_sram_reg_control{
	struct virtio_snd *snd;
	enum mtk_audio_sram_mode sram_mode;
	struct work_struct sram_reg_control;
	int result;
};
#endif


/**
 * virtsnd_kctl_info() - Returns information about the control.
 * @kcontrol: ALSA control element.
 * @uinfo: Element information.
 *
 * Context: Process context.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_kctl_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	struct virtio_snd *snd = kcontrol->private_data;
	struct virtio_kctl *kctl = &snd->kctls[kcontrol->private_value];
	struct virtio_snd_ctl_info *kinfo =
		&snd->kctl_infos[kcontrol->private_value];
	unsigned int i;

	uinfo->type = g_v2a_type_map[le32_to_cpu(kinfo->type)];
	uinfo->count = le32_to_cpu(kinfo->count);

	switch (uinfo->type) {
	case SNDRV_CTL_ELEM_TYPE_INTEGER:
		uinfo->value.integer.min =
			le32_to_cpu(kinfo->value.integer.min);
		uinfo->value.integer.max =
			le32_to_cpu(kinfo->value.integer.max);
		uinfo->value.integer.step =
			le32_to_cpu(kinfo->value.integer.step);

		break;
	case SNDRV_CTL_ELEM_TYPE_INTEGER64:
		uinfo->value.integer64.min =
			le64_to_cpu(kinfo->value.integer64.min);
		uinfo->value.integer64.max =
			le64_to_cpu(kinfo->value.integer64.max);
		uinfo->value.integer64.step =
			le64_to_cpu(kinfo->value.integer64.step);

		break;
	case SNDRV_CTL_ELEM_TYPE_ENUMERATED:
		i = uinfo->value.enumerated.item;
		if (i >= le32_to_cpu(kinfo->value.enumerated.items))
			return -EINVAL;

		uinfo->value.enumerated.items = kinfo->value.enumerated.items;
		strscpy(uinfo->value.enumerated.name, kctl->items[i].item,
			sizeof(uinfo->value.enumerated.name));

		uinfo->value.enumerated.names_ptr = (__u64)kctl->items;

		break;
	}

	return 0;
}

/**
 * virtsnd_kctl_get() - Read the value from the control.
 * @kcontrol: ALSA control element.
 * @uvalue: Element value.
 *
 * Context: Process context.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_kctl_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *uvalue)
{
	struct virtio_snd *snd = kcontrol->private_data;
	struct virtio_snd_ctl_info *kinfo =
		&snd->kctl_infos[kcontrol->private_value];
	unsigned int type = le32_to_cpu(kinfo->type);
	unsigned int count = le32_to_cpu(kinfo->count);
	struct virtio_snd_msg *msg;
	struct virtio_snd_ctl_hdr *hdr;
	struct virtio_snd_ctl_value *kvalue;
	size_t request_size = sizeof(*hdr);
	size_t response_size = sizeof(struct virtio_snd_hdr) + sizeof(*kvalue);
	unsigned int i;
	int rc;

	msg = virtsnd_ctl_msg_alloc(request_size, response_size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	virtsnd_ctl_msg_ref(msg);

	hdr = virtsnd_ctl_msg_request(msg);
	hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_READ);
	hdr->control_id = cpu_to_le32(kcontrol->private_value);

	rc = virtsnd_ctl_msg_send_sync(snd, msg);
	if (rc)
		goto on_failure;

	kvalue = (void *)((u8 *)virtsnd_ctl_msg_response(msg) +  sizeof(struct virtio_snd_hdr));

	switch (type) {
	case VIRTIO_SND_CTL_TYPE_BOOLEAN:
	case VIRTIO_SND_CTL_TYPE_INTEGER:
		for (i = 0; i < count; ++i)
			uvalue->value.integer.value[i] =
				le32_to_cpu(kvalue->value.integer[i]);
		break;
	case VIRTIO_SND_CTL_TYPE_INTEGER64:
		for (i = 0; i < count; ++i)
			uvalue->value.integer64.value[i] =
				le64_to_cpu(kvalue->value.integer64[i]);
		break;
	case VIRTIO_SND_CTL_TYPE_ENUMERATED:
		for (i = 0; i < count; ++i)
			uvalue->value.enumerated.item[i] =
				le32_to_cpu(kvalue->value.enumerated[i]);
		break;
	case VIRTIO_SND_CTL_TYPE_BYTES:
		memcpy(uvalue->value.bytes.data, kvalue->value.bytes, count);
		break;
	case VIRTIO_SND_CTL_TYPE_IEC958:
		memcpy(&uvalue->value.iec958, &kvalue->value.iec958,
		       sizeof(uvalue->value.iec958));
		break;
	}

on_failure:
	virtsnd_ctl_msg_unref(msg);

	return rc;
}

/**
 * virtsnd_kctl_put() - Write the value to the control.
 * @kcontrol: ALSA control element.
 * @uvalue: Element value.
 *
 * Context: Process context.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_kctl_put(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *uvalue)
{
	struct virtio_snd *snd = (struct virtio_snd *)kcontrol->private_data;
	struct virtio_snd_ctl_info *kinfo =
		&snd->kctl_infos[kcontrol->private_value];
	unsigned int type = le32_to_cpu(kinfo->type);
	unsigned int count = le32_to_cpu(kinfo->count);
	struct virtio_snd_msg *msg;
	struct virtio_snd_ctl_hdr *hdr;
	struct virtio_snd_ctl_value *kvalue;
	size_t request_size = sizeof(*hdr) + sizeof(*kvalue);
	size_t response_size = sizeof(struct virtio_snd_hdr);
	unsigned int i;

	msg = virtsnd_ctl_msg_alloc(request_size, response_size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = virtsnd_ctl_msg_request(msg);
	hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_WRITE);
	hdr->control_id = cpu_to_le32(kcontrol->private_value);

	kvalue = (void *)((u8 *)hdr + sizeof(*hdr));

	switch (type) {
	case VIRTIO_SND_CTL_TYPE_BOOLEAN:
	case VIRTIO_SND_CTL_TYPE_INTEGER:
		for (i = 0; i < count; ++i)
			kvalue->value.integer[i] =
				cpu_to_le32(uvalue->value.integer.value[i]);
		break;
	case VIRTIO_SND_CTL_TYPE_INTEGER64:
		for (i = 0; i < count; ++i)
			kvalue->value.integer64[i] =
				cpu_to_le64(uvalue->value.integer64.value[i]);
		break;
	case VIRTIO_SND_CTL_TYPE_ENUMERATED:
		for (i = 0; i < count; ++i)
			kvalue->value.enumerated[i] =
				cpu_to_le32(uvalue->value.enumerated.item[i]);
		break;
	case VIRTIO_SND_CTL_TYPE_BYTES:
		memcpy(kvalue->value.bytes, uvalue->value.bytes.data, count);
		break;
	case VIRTIO_SND_CTL_TYPE_IEC958:
		memcpy(&kvalue->value.iec958, &uvalue->value.iec958,
		       sizeof(kvalue->value.iec958));
		break;
	}

	return virtsnd_ctl_msg_send_sync(snd, msg);
}

/**
 * virtsnd_kctl_tlv_op() - Perform an operation on the control's metadata.
 * @kcontrol: ALSA control element.
 * @op_flag: Operation code (SNDRV_CTL_TLV_OP_XXX).
 * @size: Size of the TLV data in bytes.
 * @utlv: TLV data.
 *
 * Context: Process context.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_kctl_tlv_op(struct snd_kcontrol *kcontrol, int op_flag,
			       unsigned int size, unsigned int __user *utlv)
{
	struct virtio_snd *snd = (struct virtio_snd *)kcontrol->private_data;
	struct virtio_snd_msg *msg;
	struct virtio_snd_ctl_hdr *hdr;
	unsigned int *tlv;
	struct scatterlist sg;
	int rc;

	msg = virtsnd_ctl_msg_alloc(sizeof(*hdr), sizeof(struct virtio_snd_hdr),
				    GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	tlv = kzalloc(size, GFP_KERNEL);
	if (!tlv) {
		virtsnd_ctl_msg_unref(msg);
		return -ENOMEM;
	}

	sg_init_one(&sg, tlv, size);

	hdr = virtsnd_ctl_msg_request(msg);
	hdr->control_id = cpu_to_le32(kcontrol->private_value);

	switch (op_flag) {
	case SNDRV_CTL_TLV_OP_READ:
		hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_TLV_READ);

		rc = virtsnd_ctl_msg_send(snd, msg, NULL, &sg, false);
		if (!rc) {
			if (copy_to_user(utlv, tlv, size))
				rc = -EFAULT;
		}

		break;
	case SNDRV_CTL_TLV_OP_WRITE:
	case SNDRV_CTL_TLV_OP_CMD:
		if (op_flag == SNDRV_CTL_TLV_OP_WRITE)
			hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_TLV_WRITE);
		else
			hdr->hdr.code =
				cpu_to_le32(VIRTIO_SND_R_CTL_TLV_COMMAND);

		if (copy_from_user(tlv, utlv, size))
			rc = -EFAULT;
		else
			rc = virtsnd_ctl_msg_send(snd, msg, &sg, NULL, false);

		break;
	}

	kfree(tlv);

	return rc;
}

/**
 * virtsnd_kctl_get_enum_items() - Query items for the ENUMERATED element type.
 * @snd: VirtIO sound device.
 * @cid: Control element ID.
 *
 * This function is called during initial device initialization.
 *
 * Context: Any context that permits to sleep.
 * Return: 0 on success, -errno on failure.
 */
static int virtsnd_kctl_get_enum_items(struct virtio_snd *snd, unsigned int cid)
{
	struct virtio_device *vdev = snd->vdev;
	struct virtio_snd_ctl_info *kinfo = &snd->kctl_infos[cid];
	struct virtio_kctl *kctl = &snd->kctls[cid];
	struct virtio_snd_msg *msg;
	struct virtio_snd_ctl_hdr *hdr;
	unsigned int n = le32_to_cpu(kinfo->value.enumerated.items);
	struct scatterlist sg;
	int rc;
	struct virtio_snd_ctl_enum_item *values;

	msg = virtsnd_ctl_msg_alloc(sizeof(*hdr),
				    sizeof(struct virtio_snd_hdr), GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	// kctl->items = devm_kcalloc(&vdev->dev, n, sizeof(*kctl->items),
	//			   GFP_KERNEL);
	values = devm_kcalloc(&vdev->dev, n, sizeof(*values),
				   GFP_KERNEL);
	//asset 超过4k
	if (!values) {
		virtsnd_ctl_msg_unref(msg);
		return -ENOMEM;
	}

	sg_init_one(&sg, values, n * sizeof(*values));

	hdr = virtsnd_ctl_msg_request(msg);
	hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_ENUM_ITEMS);
	hdr->control_id = cpu_to_le32(cid);

	rc = virtsnd_ctl_msg_send(snd, msg, NULL, &sg, false);

	if (rc){
		dev_info(&vdev->dev,
			 "Failed to query enumerated information: %d\n",
			 rc);
		devm_kfree(&vdev->dev, values);
		return rc;
	}

	kctl->items = values;

	return rc;
}

/**
 * virtsnd_kctl_parse_cfg() - Parse the control element configuration.
 * @snd: VirtIO sound device.
 *
 * This function is called during initial device initialization.
 *
 * Context: Any context that permits to sleep.
 * Return: 0 on success, -errno on failure.
 */
int virtsnd_kctl_parse_cfg(struct virtio_snd *snd)
{
	struct virtio_device *vdev = snd->vdev;
	u32 i;
	int rc;

	virtio_cread_le(vdev, struct virtio_snd_config, controls,
			&snd->nkctls);

	if (!snd->nkctls)
		return 0;

	snd->kctl_infos = devm_kcalloc(&vdev->dev, snd->nkctls,
				       sizeof(*snd->kctl_infos), GFP_KERNEL);
	if (!snd->kctl_infos)
		return -ENOMEM;

	snd->kctls = devm_kcalloc(&vdev->dev, snd->nkctls, sizeof(*snd->kctls),
				  GFP_KERNEL);
	if (!snd->kctls)
		return -ENOMEM;

	rc = virtsnd_ctl_query_info(snd, VIRTIO_SND_R_CTL_INFO, 0, snd->nkctls,
				    sizeof(*snd->kctl_infos), snd->kctl_infos);
	if (rc)
		return rc;
	for (i = 0; i < snd->nkctls; ++i) {
		struct virtio_snd_ctl_info *kinfo = &snd->kctl_infos[i];
		unsigned int type = le32_to_cpu(kinfo->type);

		if (type == VIRTIO_SND_CTL_TYPE_ENUMERATED) {
			rc = virtsnd_kctl_get_enum_items(snd, i);
			if (rc)
				return rc;
		}
	}

	// TODO add sram_size kcontrol
	// TODO add aaudio_ion

	return 0;
}

/**
 * virtsnd_kctl_build_devs() - Build ALSA control elements.
 * @snd: VirtIO sound device.
 *
 * Context: Any context that permits to sleep.
 * Return: 0 on success, -errno on failure.
 */
int virtsnd_kctl_build_devs(struct virtio_snd *snd)
{
	unsigned int cid;

	for (cid = 0; cid < snd->nkctls; ++cid) {
		struct virtio_snd_ctl_info *kinfo = &snd->kctl_infos[cid];
		struct virtio_kctl *kctl = &snd->kctls[cid];
		struct snd_kcontrol_new kctl_new;
		unsigned int i;
		int rc;

		memset(&kctl_new, 0, sizeof(kctl_new));

		kctl_new.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
		kctl_new.name = kinfo->name;
		kctl_new.index = le32_to_cpu(kinfo->index);

		for (i = 0; i < ARRAY_SIZE(g_v2a_access_map); ++i)
			if (le32_to_cpu(kinfo->access) & (1 << i))
				kctl_new.access |= g_v2a_access_map[i];

		if (kctl_new.access & (SNDRV_CTL_ELEM_ACCESS_TLV_READ |
				       SNDRV_CTL_ELEM_ACCESS_TLV_WRITE |
				       SNDRV_CTL_ELEM_ACCESS_TLV_COMMAND)) {
			kctl_new.access |= SNDRV_CTL_ELEM_ACCESS_TLV_CALLBACK;
			kctl_new.tlv.c = virtsnd_kctl_tlv_op;
		}

		kctl_new.info = virtsnd_kctl_info;
		kctl_new.get = virtsnd_kctl_get;
		kctl_new.put = virtsnd_kctl_put;
		kctl_new.private_value = cid;

		kctl->kctl = snd_ctl_new1(&kctl_new, snd);
		if (!kctl->kctl)
			return -ENOMEM;

		rc = snd_ctl_add(snd->card, kctl->kctl);
		if (rc)
			return rc;
	}

	return 0;
}

/**
 * virtsnd_kctl_event() - Handle the control element event notification.
 * @snd: VirtIO sound device.
 * @event: VirtIO sound event.
 *
 * Context: Interrupt context.
 */
void virtsnd_kctl_event(struct virtio_snd *snd, struct virtio_snd_event *event)
{
	struct virtio_snd_ctl_event *kevent =
		(struct virtio_snd_ctl_event *)event;
	struct virtio_kctl *kctl;
	unsigned int cid = le16_to_cpu(kevent->control_id);
	unsigned int mask = 0;
	unsigned int i;

	if (cid >= snd->nkctls)
		return;

	for (i = 0; i < ARRAY_SIZE(g_v2a_mask_map); ++i)
		if (le16_to_cpu(kevent->mask) & (1 << i))
			mask |= g_v2a_mask_map[i];


	kctl = &snd->kctls[cid];

	snd_ctl_notify(snd->card, mask, &kctl->kctl->id);
}

#if defined(CONFIG_SND_VIRTIO_MTK_PASSTHROUGH)
/**
 * virtsnd_kctl_find_by_name() - Find control by name.
 * @snd: VirtIO sound device.
 * @name: control name.
 * @out: ptr to receive virtio control ptr.
 *
 * Return: 0 on success, -errno on failure.
 */
int virtsnd_kctl_find_by_name(struct virtio_snd *snd, const char *name,
		struct virtio_kctl **out)
{
	int rc = -ENOENT;
	int i;

	for (i = 0; i < snd->nkctls; ++i) {
		if (strcmp(snd->kctl_infos[i].name, name) == 0) {
			*out = &snd->kctls[i];
			rc = 0;
			break;
		}
	}

	return rc;
}

int virtsnd_info_find_by_name(struct virtio_snd *snd, const char *name,
		struct virtio_snd_ctl_info **out)
{
	int rc = -ENOENT;
	int i;

	for (i = 0; i < snd->nkctls; ++i) {
		if (strcmp(snd->kctl_infos[i].name, name) == 0) {
			*out = &snd->kctl_infos[i];
			rc = i;
			break;
		}
	}

	return rc;
}

int virtsnd_kctl_get_use_dram_only(struct virtio_snd *snd, int pcm_id)
{
	int rc;
	struct snd_ctl_elem_value *kval;

	kval = kmalloc(sizeof(*kval), GFP_KERNEL);
	if (!kval)
		return -ENOMEM;
	rc = virtsnd_kctl_get(snd->remote_use_dram_only_ctl->kctl, kval);
	if (rc) {
		dev_info(&snd->vdev->dev, "get remote use_dram_only failed: %d\n", rc);
		kfree(kval);
		rc = -EIO;
	} else {
		rc = kval->value.bytes.data[pcm_id];
	}
	kfree(kval);
	return rc;
}

int virtsnd_kctl_reg_shm(struct virtio_snd *snd, int pcm_id,
		dma_addr_t dma_area, size_t bytes, int sram)
{
	int rc;
	struct virtio_snd_ctl_info *kinfo;
	unsigned int type;
	unsigned int count;
	struct virtio_snd_msg *msg;
	struct virtio_snd_ctl_hdr *hdr;
	struct virtio_snd_ctl_value *kvalue;
	struct virtio_passthrough_shm_msg *shm;
	size_t request_size = sizeof(*hdr) + sizeof(*kvalue);
	size_t response_size = sizeof(struct virtio_snd_hdr);

	rc = virtsnd_info_find_by_name(snd, "passthrough_shm",
			&kinfo);
	if (rc < 0){
		dev_info(&snd->vdev->dev, "get passthrough_shm info failed: %d\n", rc);
		return rc;
	}

	type = le32_to_cpu(kinfo->type);
	count = le32_to_cpu(kinfo->count);

	msg = virtsnd_ctl_msg_alloc(request_size, response_size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = virtsnd_ctl_msg_request(msg);
	hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_WRITE);
	hdr->control_id = cpu_to_le32(kinfo->index);
	kvalue = (void *)((u8 *)hdr + sizeof(*hdr));
	shm = (struct virtio_passthrough_shm_msg *) kvalue->value.bytes;
	shm->cmd = sram ?
			VIRTIO_PASSTHROUGH_SHM_CMD_REG_SRAM
			: VIRTIO_PASSTHROUGH_SHM_CMD_REG_DRAM;
	shm->pcm_id = pcm_id;
	shm->pa = dma_area;
	shm->bytes = bytes;

	rc = virtsnd_ctl_msg_send_sync(snd, msg);
	return rc;
}

int virtsnd_kctl_unreg_shm(struct virtio_snd *snd, int pcm_id, int sram)
{
	int rc;

	struct virtio_snd_ctl_info *kinfo;
	unsigned int type;
	unsigned int count;
	struct virtio_snd_msg *msg;
	struct virtio_snd_ctl_hdr *hdr;
	struct virtio_snd_ctl_value *kvalue;
	struct virtio_passthrough_shm_msg *shm;
	size_t request_size = sizeof(*hdr) + sizeof(*kvalue);
	size_t response_size = sizeof(struct virtio_snd_hdr);

	rc = virtsnd_info_find_by_name(snd, "passthrough_shm",
			&kinfo);
	if (rc < 0){
		dev_info(&snd->vdev->dev, "get passthrough_shm info failed: %d\n", rc);
		return rc;
	}

	type = le32_to_cpu(kinfo->type);
	count = le32_to_cpu(kinfo->count);

	msg = virtsnd_ctl_msg_alloc(request_size, response_size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = virtsnd_ctl_msg_request(msg);
	hdr->hdr.code = cpu_to_le32(VIRTIO_SND_R_CTL_WRITE);
	hdr->control_id = cpu_to_le32(kinfo->index);
	kvalue = (void *)((u8 *)hdr + sizeof(*hdr));

	shm = (struct virtio_passthrough_shm_msg *) kvalue->value.bytes;
	shm->cmd = sram ?
			VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_SRAM
			: VIRTIO_PASSTHROUGH_SHM_CMD_UNREG_DRAM;
	shm->pcm_id = pcm_id;
	shm->pa = 0;
	shm->bytes = 0;
	rc = virtsnd_ctl_msg_send_sync(snd, msg);
	return rc;
}

int virtio_set_sram_mode(struct device *dev,
				enum mtk_audio_sram_mode sram_mode)
{

	int rc = 0;
	struct virtio_device *vdev = dev_to_virtio(dev);
	struct virtio_snd *snd = vdev->priv;

	snd->sram_mode = sram_mode;
	snd->result = -ENOMEM;


	schedule_work(&snd->sram_reg_control);
	rc = snd->result;

	return rc;
}
#endif
