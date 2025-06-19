/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of ILITEK CommonFlow
 *
 * Copyright (c) 2022 ILI Technology Corp.
 * Copyright (c) 2022 Luca Hsu <luca_hsu@ilitek.com>
 * Copyright (c) 2022 Joe Hung <joe_hung@ilitek.com>
 */

#include "ilitek_update.h"

char g_info[4096];
void *g_private;
update_info_t g_update_info = NULL;

#ifndef ILITEK_KERNEL_DRIVER
static int hex_to_bin(uint8_t ch)
{
	uint8_t cu = ch & 0xdf;
	return -1 +
		((ch - '0' +  1) & (unsigned)((ch - '9' - 1) &
		('0' - 1 - ch)) >> 8) +
		((cu - 'A' + 11) & (unsigned)((cu - 'F' - 1) &
		('A' - 1 - cu)) >> 8);
}

static int hex2bin(uint8_t *dst, const uint8_t *src, size_t count)
{
	int hi = 0, lo = 0;

	while (count--) {
		if ((hi = hex_to_bin(*src++)) < 0 ||
		    (lo = hex_to_bin(*src++)) < 0) {
			TP_ERR("hex_to_bin failed, hi: %d, lo: %d\n", hi, lo);
			return -EINVAL;
		}

		*dst++ = (hi << 4) | lo;
	}
	return 0;
}
#endif

static uint32_t get_tag_addr(uint32_t start, uint32_t end,
			     const uint8_t *buf, unsigned int buf_size,
			     const uint8_t *tag, unsigned int tag_size)
{
	unsigned int i;

	for (i = start; i <= end - tag_size && i < buf_size - tag_size; i++) {
		if (!memcmp(buf + i, tag, tag_size))
			return i + tag_size + 1;
	}

	return end;
}

static uint32_t get_endaddr(uint32_t start, uint32_t end, const uint8_t *buf,
			    unsigned int buf_size, bool is_AP)
{
	uint32_t addr;
	uint8_t tag[32];
	const uint8_t ap_tag[] = "ILITek AP CRC   ";
	const uint8_t blk_tag[] = "ILITek END TAG  ";

	memset(tag, 0xFF, sizeof(tag));
	memcpy(tag + 16, (is_AP) ? ap_tag : blk_tag, 16);

	addr = get_tag_addr(start, end, buf, buf_size, tag, sizeof(tag));
	TP_DBG("find tag in start/end: 0x%x/0x%x, tag addr: 0x%x\n",
		start, end, addr);

	return addr;
}

static void decode_mm(struct ilitek_fw_handle *fw, uint32_t addr, uint8_t *buf)
{
	uint8_t i;
	union mapping_info *mapping;

	UPDATE_UI_INFO("------------Memory Mapping information------------\n");

	UPDATE_UI_INFO("memory-mapping-info addr: %#x\n", addr);

	mapping = (union mapping_info *)(buf + addr);

	UPDATE_UI_INFO("Hex Mapping Ver.: 0x%x\n", le32(mapping->mapping_ver, 3));
	UPDATE_UI_INFO("Hex Protocol: 0x%x\n", le32(mapping->protocol_ver, 3));
	UPDATE_UI_INFO("Hex IC type: 0x%x\n", mapping->ic_name);

	fw->file_ic_name = mapping->ic_name;

	if (le32(mapping->mapping_ver, 3) < 0x10000)
		goto memory_mapping_end;

	UPDATE_UI_INFO("Hex FW Ver(MSB 4-bytes): %02x-%02x-%02x-%02x\n",
			mapping->_lego.fw_ver[3], mapping->_lego.fw_ver[2],
			mapping->_lego.fw_ver[1], mapping->_lego.fw_ver[0]);
	UPDATE_UI_INFO("Hex Tuning Ver.(4-bytes): %02x-%02x-%02x-%02x\n",
			mapping->_lego.tuning_ver[3], mapping->_lego.tuning_ver[2],
			mapping->_lego.tuning_ver[1], mapping->_lego.tuning_ver[0]);

	fw->file_fw_ver[0] = mapping->_lego.fw_ver[3];
	fw->file_fw_ver[1] = mapping->_lego.fw_ver[2];
	fw->file_fw_ver[2] = mapping->_lego.fw_ver[1];
	fw->file_fw_ver[3] = mapping->_lego.fw_ver[0];

	fw->block_num = mapping->_lego.block_num;
	if (fw->block_num > ARRAY_SIZE(fw->blocks)) {
		TP_ERR("Unexpected block num: %hhu\n", fw->block_num);
		fw->block_num = ARRAY_SIZE(fw->blocks);
	}

	UPDATE_UI_INFO("Total %hhu blocks\n", fw->block_num);
	for (i = 0; i < fw->block_num; i++) {
		fw->blocks[i].start = le32(mapping->_lego.blocks[i].addr, 3);
		fw->blocks[i].end = (i == fw->block_num - 1) ?
			le32(mapping->_lego.end_addr, 3) :
			le32(mapping->_lego.blocks[i + 1].addr, 3);

		fw->blocks[i].end = get_endaddr(fw->blocks[i].start,
						fw->blocks[i].end,
						buf, ILITEK_FW_BUF_SIZE,
						i == 0);

		UPDATE_UI_INFO("Block[%u], start:%#x end:%#x\n",
				i, fw->blocks[i].start, fw->blocks[i].end);
	}

memory_mapping_end:
	UPDATE_UI_INFO("--------------------------------------------------\n");
}

static int decode_hex(struct ilitek_fw_handle *fw, uint8_t *hex,
		      uint32_t start, uint32_t end, uint8_t *fw_buf)
{
	int error;
	uint8_t info[4], data[16];
	unsigned int i, len, addr, type, exaddr = 0;
	uint32_t mapping_info_addr = 0;

	fw->ap.start = (~0U);
	fw->ap.end = 0x0;
	fw->ap.checksum = 0x0;
	fw->df.start = (~0U);
	fw->df.end = 0x0;
	fw->df.checksum = 0x0;

	for (i = start; i < end; i++) {
		/* filter out non-hexadecimal characters */
		if (hex_to_bin(hex[i]) < 0)
			continue;

		if ((error = hex2bin(info, hex + i, sizeof(info))) < 0)
			return error;

		len = info[0];
		addr = be32(info + 1, 2);
		type = info[3];

		if ((error = hex2bin(data, hex + i + 8, len)) < 0)
			return error;

		switch (type) {
		case 0xAC:
			mapping_info_addr = be32(data, len);
			break;

		case 0xAD:
			fw->df.start = be32(data, len);
			break;

		case 0xBA:
			if (be32(data, len) != 2U)
				break;

			TP_MSG("start to decode M2V part of hex file\n");
			fw->m2v = true;
			return decode_hex(fw, hex, i + 10 + len * 2 + 1, end,
					  fw->m2v_buf);

		case 0x01:
			goto success_return;

		case 0x02:
			exaddr = be32(data, len) << 4;
			break;

		case 0x04:
			exaddr = be32(data, len) << 16;
			break;

		case 0x05:
			TP_MSG("hex data type: %#x, start linear address: %#x\n",
				type, be32(data, len));
			break;

		case 0x00:
			addr += exaddr;

			if (addr + len > ILITEK_FW_BUF_SIZE) {
				TP_ERR("hex addr: %#x, buf size: %#x OOB\n",
					addr + len, ILITEK_FW_BUF_SIZE);
				return -ENOBUFS;
			}
			memcpy(fw_buf + addr, data, len);

			fw->ap.start = MIN(fw->ap.start, addr);

			if (addr + len < fw->df.start) {
				fw->ap.end = MAX(fw->ap.end, addr + len);
				fw->ap.checksum += get_checksum(0, len, data,
								sizeof(data));
			} else {
				fw->df.end = MAX(fw->df.end, addr + len);
				fw->df.checksum += get_checksum(0, len, data,
								sizeof(data));
			}

			break;
		default:
			TP_ERR("unexpected type:%#x in hex, len:%u, addr:%#x\n",
				type, len, addr);
			return -EINVAL;
		}

		i = i + 10 + len * 2;
	}

success_return:
	if (mapping_info_addr)
		decode_mm(fw, mapping_info_addr, fw_buf);

	return 0;
}

static int decode_bin(struct ilitek_fw_handle *fw, uint8_t *bin, int bin_size)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;

	fw->ap.start = (~0U);
	fw->ap.end = 0x0;
	fw->ap.checksum = 0x0;
	fw->df.start = (~0U);
	fw->df.end = 0x0;
	fw->df.checksum = 0x0;

	if (bin_size > ILITEK_FW_BUF_SIZE) {
		TP_ERR("bin file size: %#x, buf size: %#x OOB\n", bin_size,
			ILITEK_FW_BUF_SIZE);
		return -ENOBUFS;
	}
	memcpy(fw->fw_buf, bin, bin_size);

	if ((error = api_protocol_set_cmd(dev, GET_PTL_VER, NULL)) < 0)
		return error;

	switch (dev->protocol.flag) {
	case PTL_V6:
		if ((error = api_protocol_set_cmd(dev, GET_MCU_VER,
						  NULL)) < 0 ||
		    (error = api_protocol_set_cmd(dev, GET_MCU_INFO,
						  NULL)) < 0)
			return error;

		decode_mm(fw, dev->kernel_info.mm_addr, bin);
		break;

	case PTL_V3:
		switch (dev->mcu.ic_name) {
		case 0x2312:
		case 0x2315:
			fw->df.start = 0x1f000;
			decode_mm(fw, 0x0500, bin);
			fw->ap.start = 0x0;
			fw->ap.end = get_endaddr(fw->ap.start, fw->df.start,
						 bin, bin_size, true) + 3;
			break;
		default:
			fw->df.start = 0xf000;
			decode_mm(fw, 0x2020, bin);
			fw->ap.start = 0x2000;
			fw->ap.end = get_endaddr(fw->ap.start, fw->df.start,
						 bin, bin_size, true) + 1;
			break;
		}
		fw->ap.checksum = get_checksum(fw->ap.start, fw->ap.end, bin,
					       bin_size);
		fw->df.end = bin_size;
		fw->df.checksum = get_checksum(fw->df.start, fw->df.end, bin,
					       bin_size);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef ILITEK_BOOT_UPDATE
#include "ilitek_fw.h"

static int decode_ili(struct ilitek_fw_handle *fw)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	uint32_t i;
	bool need_update = false;
	int size;

	/* for boot upgrade without using .ili file */
	size = sizeof(CTPM_FW);
	if (size < 32)
		return -EINVAL;

	memcpy(fw->file_fw_ver, CTPM_FW + 18, 8);

	TP_MSG_ARR("IC  fw ver:", 8, dev->fw_ver);
	TP_MSG_ARR("Hex fw ver:", 8, fw->file_fw_ver);

	if (dev->ic[0].mode == 0x5A) {
		for (i = 0; i < 8; i++) {
			if (fw->file_fw_ver[i] > dev->fw_ver[i]) {
				need_update = true;
				break;
			}

			if (fw->file_fw_ver[i] < dev->fw_ver[i])
				break;
		}

		if (!need_update) {
			TP_MSG("File FW version is older, no need to update\n");
			return -EEXIST;
		}
	}

	if ((error = decode_bin(fw, CTPM_FW + 32, size - 32)) < 0)
		return error;

	if (dev->protocol.flag == PTL_V6)
		return 0;

	/*
	 * Only V3 2312/2315 need ap checksum,
	 * V3 251x need ap crc, which would be re-calculated afterward.
	 * .ili file fill DF section with default 0xFF, but not 0.
	 * remove below CTPM_FW parser after fixing .ili converter bug.
	 */
	if (dev->mcu.ic_name == 0x2312 || dev->mcu.ic_name == 0x2315) {
		fw->ap.start = (CTPM_FW[0] << 16) + (CTPM_FW[1] << 8) +
				CTPM_FW[2];
		fw->ap.end = (CTPM_FW[3] << 16) + (CTPM_FW[4] << 8) +
				CTPM_FW[5];
		fw->ap.checksum = (CTPM_FW[6] << 16) + (CTPM_FW[7] << 8) +
				CTPM_FW[8];
	}
	fw->df.end = (CTPM_FW[12] << 16) + (CTPM_FW[13] << 8) + CTPM_FW[14];
	fw->df.checksum = (CTPM_FW[15] << 16) + (CTPM_FW[16] << 8) +
				CTPM_FW[17];
	fw->file_ic_name = CTPM_FW[10] + (CTPM_FW[11] << 8);
	TP_MSG("ILI IC type: 0x%x\n", fw->file_ic_name);

	return 0;
}
#endif

static int decode_firmware(struct ilitek_fw_handle *fw, char *file_name)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	char *file_ext;
	int file_size = 0;
	uint8_t *file_buf;

	uint32_t i;

	if (!fw)
		return -EINVAL;

	fw->ap.start = (~0U);
	fw->ap.end = 0x0;
	fw->ap.checksum= 0x0;
	fw->df.start = (~0U);
	fw->df.end = 0x0;
	fw->df.checksum= 0x0;

	if (!(file_ext = strrchr(file_name, '.')))
		return -ENOENT;

	file_buf = (uint8_t *)CALLOC(ILITEK_FW_FILE_SIZE, 1);
	if (!file_buf)
		return -ENOMEM;

	/* no need to read .ili file */
	if (_strcasecmp(file_ext, ".ili")) {
		TP_MSG("start to load file: %s\n", file_name);
		file_size = fw->cb.read_fw(file_name, file_buf,
					   ILITEK_FW_FILE_SIZE, fw->_private);

		if ((error = file_size) < 0) {
			TP_ERR("read fw file failed, err: %d\n", error);
			goto err_free;
		}
	}

	_strcpy(fw->fw_name, file_name, sizeof(fw->fw_name));

	if (!_strcasecmp(file_ext, ".hex"))
		error = decode_hex(fw, file_buf, 0, file_size, fw->fw_buf);
	else if (!_strcasecmp(file_ext, ".bin"))
		error = decode_bin(fw, file_buf, file_size);
#ifdef ILITEK_BOOT_UPDATE
	else if (!_strcasecmp(file_ext, ".ili"))
		error = decode_ili(fw);
#endif
	else
		error = -EINVAL;

	if (error < 0)
		goto err_free;

	/* for V3 251x IC, need re-calculate AP CRC (not checksum) */
	if (dev->protocol.flag == PTL_V3 &&
	    dev->mcu.ic_name != 0x2312 &&
	    dev->mcu.ic_name != 0x2315) {
		fw->ap.checksum = get_crc(fw->ap.start, fw->ap.end - 2,
					  fw->fw_buf, ILITEK_FW_BUF_SIZE);
	}

	/* for Lego and V6 IC, check block's start/end address validity */
	if (dev->protocol.flag == PTL_V6) {
		for (i = 0; i < fw->block_num; i++) {
			if (dev->kernel_info.min_addr <= fw->blocks[i].start &&
			    dev->kernel_info.max_addr > fw->blocks[i].end)
				continue;

			if (!(fw->blocks[i].start % 0x1000))
				continue;

			TP_ERR("Block[%u] addr. OOB (%#x <= %#x/%#x < %#x) or invalid start addr\n",
				i, dev->kernel_info.min_addr,
				fw->blocks[i].start, fw->blocks[i].end,
				dev->kernel_info.max_addr);
			error = -EINVAL;
			goto err_free;
		}
	}

err_free:
	CFREE(file_buf);

	TP_MSG("MCU version: 0x%04x, Hex IC type: 0x%04x\n",
		dev->mcu.ic_name, fw->file_ic_name);
	if (dev->mcu.ic_name != fw->file_ic_name &&
	    (dev->mcu.ic_name & 0xFF00) != 0x0300 &&
	    (dev->mcu.ic_name & 0xFF00) != 0x0900) {
		TP_ERR("ic: ILI%04x, hex: ILI%04x or ILI230x not matched\n",
			dev->mcu.ic_name, fw->file_ic_name);
		return -EINVAL;
	}

	return error;
}

static bool need_fw_update_v3(struct ilitek_fw_handle *fw)
{
	struct ilitek_ts_device *dev = fw->dev;
	bool need;

	UPDATE_UI_INFO("------------V3 AP/DF Info.------------\n");

	UPDATE_UI_INFO("[ap] start:0x%X, end:0x%X, checksum:0x%X\n",
		       fw->ap.start, fw->ap.end, fw->ap.checksum);
	UPDATE_UI_INFO("[df] start:0x%X, end:0x%X, checksum:0x%X\n",
		       fw->df.start, fw->df.end, fw->df.checksum);

	need = (dev->ap_crc_v3 != fw->ap.checksum ||
		(fw->df.start < fw->df.end &&
		 dev->df_crc_v3 != fw->df.checksum));

	UPDATE_UI_INFO("AP (IC:%#X/FW:%#X), Data (IC:%#X/FW:%#X) %s\n",
		       dev->ap_crc_v3, fw->ap.checksum,
		       dev->df_crc_v3, fw->df.checksum,
		       (need) ? "not matched" : "matched");

	if (dev->ic[0].mode == 0x55)
		return true;

	return need;
}

static bool need_fw_update_v6(struct ilitek_fw_handle *fw)
{
	struct ilitek_ts_device *dev = fw->dev;
	uint8_t i;
	bool need = false;

	UPDATE_UI_INFO("------------Lego Block Info.------------\n");

	/* first block CRC should be got by AP CRC command */
	for (i = 1; i < fw->block_num; i++) {
		dev->ic[0].crc[i] = api_get_block_crc_by_addr(dev,
			CRC_CALCULATE, fw->blocks[i].start, fw->blocks[i].end);
	}

	for (i = 0; i < fw->block_num; i++) {
		fw->blocks[i].crc = get_crc(fw->blocks[i].start,
					    fw->blocks[i].end - 1,
					    fw->fw_buf, ILITEK_FW_BUF_SIZE);

		fw->blocks[i].crc_match = (fw->setting.force_update) ?
			false : (dev->ic[0].crc[i] == fw->blocks[i].crc);

		need = (!fw->blocks[i].crc_match) ? true : need;

		UPDATE_UI_INFO("Block[%hhu]: start/end addr: %#x/%#x, CRC IC/File: 0x%hx/0x%hx %s\n",
			       i, fw->blocks[i].start, fw->blocks[i].end,
			       dev->ic[0].crc[i], fw->blocks[i].crc,
			       (fw->blocks[i].crc_match) ?
			       "matched" : "not matched");
	}

	/* check BL mode firstly before AP-cmd related varaible, ex: ic_num */
	if (dev->ic[0].mode == 0x55)
		return true;

	for (i = 1; i < dev->tp_info.ic_num; i++) {
		UPDATE_UI_INFO("Master/Slave[%hhu] CRC 0x%hx/0x%hx %s, mode: 0x%hhx %s\n",
			i, fw->blocks[0].crc, dev->ic[i].crc[0],
			(fw->blocks[0].crc == dev->ic[i].crc[0]) ?
			"matched" : "not matched",
			dev->ic[i].mode, dev->ic[i].mode_str);

		if (dev->ic[i].crc[0] == fw->blocks[0].crc &&
		    dev->ic[i].mode == 0x5A)
			continue;
		need = true;
	}

	if (fw->m2v) {
		api_access_slave(dev, 0x80, M2V_GET_CHECKSUM,
				 &fw->m2v_checksum);

		UPDATE_UI_INFO("M2V file/m2v checksum: %#x/%#x %s\n",
			       fw->ap.checksum, fw->m2v_checksum,
			       (fw->ap.checksum == fw->m2v_checksum) ?
			       "matched" : "not matched");

		fw->m2v_need_update = (fw->ap.checksum != fw->m2v_checksum) ||
			fw->setting.force_update;
		need = fw->m2v_need_update;
	}

	return need;
}

static bool need_fw_update(struct ilitek_fw_handle *fw)
{
	struct ilitek_ts_device *dev = fw->dev;
	bool need = false;
	int i;

	if (dev->protocol.flag == PTL_V3)
		need = need_fw_update_v3(fw);
	else if (dev->protocol.flag == PTL_V6)
		need = need_fw_update_v6(fw);

	if (fw->setting.force_update)
		return true;
	else if (fw->setting.fw_check_only)
		return false;

	for (i = 0; fw->setting.fw_ver_check && i < 8; i++) {
		if (dev->fw_ver[i] >= fw->setting.fw_ver[i])
			continue;

		UPDATE_UI_INFO("IC FW version is older than Preset FW version\n");
		UPDATE_UI_INFO("IC FW version: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X",
			       dev->fw_ver[0], dev->fw_ver[1],
			       dev->fw_ver[2], dev->fw_ver[3],
			       dev->fw_ver[4], dev->fw_ver[5],
			       dev->fw_ver[6], dev->fw_ver[7]);
		UPDATE_UI_INFO("Preset FW version: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X",
			       fw->setting.fw_ver[0], fw->setting.fw_ver[1],
			       fw->setting.fw_ver[2], fw->setting.fw_ver[3],
			       fw->setting.fw_ver[4], fw->setting.fw_ver[5],
			       fw->setting.fw_ver[6], fw->setting.fw_ver[7]);

		need = true;
		break;
	}

	return need;
}

static int update_master(struct ilitek_fw_handle *fw, int idx, uint32_t len)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	unsigned int i;
	uint16_t file_crc, ic_crc;
	int retry = 3;

	TP_MSG("updating block[%d] len: %u, start/end addr: %#x/%#x\n",
		idx, len, fw->blocks[idx].start, fw->blocks[idx].end);

err_retry:
	if ((error = api_write_enable_v6(dev, false, false,
					 fw->blocks[idx].start,
					 fw->blocks[idx].end)) < 0)
		return error;

	memset(dev->wbuf, 0xff, sizeof(dev->wbuf));
	for (i = fw->blocks[idx].start; i < fw->blocks[idx].end; i += len) {
		/*
		 * check end addr. of data write buffer is within valid range.
		 */
		if (i + len > END_ADDR_LEGO) {
			TP_ERR("block[%d] write addr. %#x + %#x > %#x OOB\n",
				idx, i, len, END_ADDR_LEGO);
			return -EINVAL;
		}

		memcpy(dev->wbuf + 1, fw->fw_buf + i, len);
		error = api_write_data_v6(dev, len + 1);

		if (error < 0) {
			if (retry-- > 0)
				goto err_retry;
			return error;
		}

		fw->progress_curr = MIN(i + len - fw->blocks[idx].offset,
					fw->progress_max);
		fw->progress = (100 * fw->progress_curr) / fw->progress_max;
		TP_DBG("block[%d] update progress: %hhu%%\n",
			idx, fw->progress);

		if (fw->cb.update_progress)
			fw->cb.update_progress(fw->progress, fw->_private);
	}

	file_crc = get_crc(fw->blocks[idx].start, fw->blocks[idx].end - 1,
			   fw->fw_buf, ILITEK_FW_BUF_SIZE);
	ic_crc = api_get_block_crc_by_addr(dev, CRC_GET, fw->blocks[idx].start,
					   fw->blocks[idx].end);

	TP_MSG("Block[%d]: start/end addr: %#x/%#x, CRC IC/File: %#x/%#x %s\n",
		idx, fw->blocks[idx].start, fw->blocks[idx].end, ic_crc,
		file_crc, (file_crc == ic_crc) ? "matched" : "not matched");

	return (file_crc != ic_crc) ? -EINVAL : 0;
}

static int update_slave(struct ilitek_fw_handle *fw)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	uint8_t i;

	for (i = 0; i < fw->block_num; i++) {
		dev->ic[0].crc[i] = api_get_block_crc_by_addr(dev,
			CRC_CALCULATE, fw->blocks[i].start, fw->blocks[i].end);
	}

	if ((error = api_protocol_set_cmd(dev, GET_AP_CRC,
					  &dev->tp_info.ic_num)) < 0)
			return error;

	for (i = 0; i < dev->tp_info.ic_num; i++) {
		if (dev->ic[0].crc[0] == dev->ic[i].crc[0] &&
		    !fw->setting.force_update)
			continue;

		TP_MSG("Master/Slave[%hhu] CRC:0x%hx/0x%hx not matched or force update\n",
			i, dev->ic[0].crc[0], dev->ic[i].crc[0]);
		TP_MSG("start updating slave...\n");
		if ((error = api_access_slave(dev, 0x3, SLAVE_WRITE_AP,
					      NULL)) < 0 ||
		    (error = api_write_enable_v6(dev, false, true,
						 fw->blocks[0].start,
						 fw->blocks[0].end)) < 0)
			return error;

		goto success_return;
	}

	if ((error = api_protocol_set_cmd(dev, GET_MCU_MOD,
					  &dev->tp_info.ic_num)) < 0)
		return error;

	for (i = 0; i < dev->tp_info.ic_num; i++) {
		if (dev->ic[i].mode == 0x5A &&
		    !fw->setting.force_update)
			continue;

		TP_MSG("Slave[%hhu] Mode:0x%hhx not in AP or force update\n",
			i, dev->ic[i].mode);
		TP_MSG("start changing slave mode to AP...\n");
		if ((error = api_access_slave(dev, 0x3, SLAVE_SET_AP,
					      NULL)) < 0)
			return  error;
		break;
	}

success_return:
	return 0;
}

static int update_M3_M2V(struct ilitek_fw_handle *fw, uint32_t len)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	uint32_t i, tmp;
	uint8_t buf[6];

	//TODO: [Joe] read garbage ?
	api_protocol_set_cmd(dev, GET_PTL_VER, NULL);
	api_protocol_set_cmd(dev, GET_PTL_VER, NULL);

	tmp = (fw->ap.end % len) ? (len - (fw->ap.end % len)) * 0xFF : 0;
	fw->ap.checksum += tmp;

	if ((error = api_to_bl_mode_m2v(dev, true)) < 0)
		return error;

	if ((error = api_set_data_len(dev, len)) < 0)
		return error;

	TP_MSG("updating M2V with start/end addr: %#x/%#x, checksum: %#x\n",
		fw->ap.start, fw->ap.end, fw->ap.checksum);

	buf[0] = (fw->ap.end >> 16) & 0xFF;
	buf[1] = (fw->ap.end >> 8) & 0xFF;
	buf[2] = fw->ap.end & 0xFF;
	buf[3] = (fw->ap.checksum>> 16) & 0xFF;
	buf[4] = (fw->ap.checksum >> 8) & 0xFF;
	buf[5] = fw->ap.checksum & 0xFF;
	if ((error = api_access_slave(dev, 0x80, M2V_WRITE_ENABLE, buf)) < 0)
		return error;

	memset(dev->wbuf, 0xff, sizeof(dev->wbuf));
	for (i = fw->ap.start; i < fw->ap.end; i += len) {
		memcpy(dev->wbuf + 1, fw->m2v_buf + i, len);

		if ((error = api_write_data_m2v(dev, len + 1)) < 0)
			return  error;

		fw->progress_curr = MIN(fw->progress_curr + len,
					fw->progress_max);
		fw->progress = (100 * fw->progress_curr) / fw->progress_max;
		TP_DBG("m2v update progress: %hhu%%\n", fw->progress);

		if (fw->cb.update_progress)
			fw->cb.update_progress(fw->progress, fw->_private);
	}

	//TODO: [Joe] check checksum again ?

	if ((error = api_to_bl_mode_m2v(dev, false)) < 0)
		return error;

	if ((error = api_access_slave(dev, 0x80, M2V_GET_FW_VER,
				      fw->m2v_fw_ver)) < 0)
		return error;

	TP_MSG_ARR("update M2V success, fw version:", 8, fw->m2v_fw_ver);

	return 0;
}

static int ilitek_update_BL_v1_8(struct ilitek_fw_handle *fw)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	uint8_t i;

	if ((error = api_set_data_len(dev, UPDATE_LEN)) < 0)
		return error;

	for (i = 0; i < fw->block_num; i++) {
		if (fw->blocks[i].crc_match)
			continue;

		if ((error = update_master(fw, i, UPDATE_LEN)) < 0) {
			TP_ERR("Upgrade Block:%hhu failed, err: %d\n",
				i, error);
			return error;
		}
	}

	if ((error = api_to_bl_mode(dev, false, fw->blocks[0].start,
				    fw->blocks[0].end)) < 0)
		return error;

	if ((error = api_set_ctrl_mode(dev, mode_suspend, false)) < 0)
		return error;

	/* get tp info. for updating ic num */
	if ((error = api_protocol_set_cmd(dev, GET_TP_INFO, NULL)) < 0)
		return error;

	if (dev->tp_info.ic_num > 1 && (error = update_slave(fw)) < 0) {
		TP_ERR("upgrade slave failed, err: %d\n", error);
		return error;
	}

	if (fw->m2v && fw->m2v_need_update &&
	    (error = update_M3_M2V(fw, 1024)) < 0) {
		TP_ERR("upgrade m2v slave failed, err: %d\n", error);
		return error;
	}

	return 0;
}

static int ilitek_update_BL_v1_7(struct ilitek_fw_handle *fw)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	uint32_t crc;
	unsigned int i;

	if ((error = api_erase_data_v3(dev)) < 0)
		return error;

	if (fw->df.end > fw->df.start) {
		TP_MSG("updating DF start/end addr.: %#x/%#x, file checksum: %#x\n",
			fw->df.start, fw->df.end, fw->df.checksum);

		if ((error = api_write_enable_v3(dev, false, false, fw->df.end,
						 fw->df.checksum)) < 0 ||
		    (error = api_check_busy(dev, 1000, 10)) < 0)
			return error;

		for (i = fw->df.start; i < fw->df.end; i += 32) {
			memset(dev->wbuf + 1, 0, 32);
			if (i + 32 < fw->df.end)
				memcpy(dev->wbuf + 1, fw->fw_buf + i, 32);
			else
				memcpy(dev->wbuf + 1, fw->fw_buf + i,
				       fw->df.end - i - 1);

			if ((error = api_write_data_v3(dev)) < 0 ||
			    (error = api_check_busy(dev, 1000, 10)) < 0)
				return error;

			fw->progress_curr += 32;
			fw->progress_curr = MIN(fw->progress_curr,
				fw->progress_max);
			fw->progress = (100 * fw->progress_curr) /
				fw->progress_max;
			TP_DBG("DF update progress: %hhu%%", fw->progress);

			if (fw->cb.update_progress)
				fw->cb.update_progress(fw->progress,
						       fw->_private);
		}

		dev->cb.delay_ms(50);

		dev->wbuf[0] = CMD_GET_AP_CRC;
		if ((error = dev->cb.write_then_read(dev->wbuf, 1, NULL, 0,
						     dev->_private)) < 0 ||
		    (error = api_check_busy(dev, 1000, 10)) < 0)
			return error;

		dev->wbuf[0] = CMD_GET_AP_CRC;
		if ((error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 4,
						    dev->_private)) < 0)
			return error;

		crc = (le16(dev->rbuf + 2) << 16) | le16(dev->rbuf);
		TP_MSG("DF CRC file:%#x/ic:%#x %s\n", fw->df.checksum, crc,
			(crc == fw->df.checksum) ? "matched" : "not matched");

		if (crc != fw->df.checksum)
			return -EFAULT;
	}

	if (fw->ap.end > fw->ap.start) {
		TP_MSG("updating AP start/end addr.: %#x/%#x, file crc: %#x\n",
			fw->ap.start, fw->ap.end, fw->ap.checksum);

		if ((error = api_write_enable_v3(dev, false, true, fw->ap.end,
						 fw->ap.checksum)) < 0 ||
		    (error = api_check_busy(dev, 1000, 10)) < 0)
			return error;

		for (i = fw->ap.start; i < fw->ap.end; i += 32) {
			memcpy(dev->wbuf + 1, fw->fw_buf + i, 32);
			if ((error = api_write_data_v3(dev)) < 0 ||
			    (error = api_check_busy(dev, 1000, 10)) < 0)
				return error;

			fw->progress_curr += 32;
			fw->progress_curr = MIN(fw->progress_curr,
				fw->progress_max);
			fw->progress = (100 * fw->progress_curr) /
				fw->progress_max;
			TP_DBG("AP update progress: %hhu%%", fw->progress);

			if (fw->cb.update_progress)
				fw->cb.update_progress(fw->progress,
						       fw->_private);
		}

		dev->wbuf[0] = CMD_GET_AP_CRC;
		if ((error = dev->cb.write_then_read(dev->wbuf, 1, NULL, 0,
						     dev->_private)) < 0 ||
		    (error = api_check_busy(dev, 1000, 10)) < 0)
			return error;

		dev->wbuf[0] = CMD_GET_AP_CRC;
		if ((error = dev->cb.write_then_read(dev->wbuf, 1, dev->rbuf, 4,
						    dev->_private)) < 0)
			return error;

		crc = (le16(dev->rbuf + 2) << 16) | le16(dev->rbuf);
		TP_MSG("AP CRC file:%#x/ic:%#x %s\n", fw->ap.checksum, crc,
			(crc == (fw->ap.checksum & 0xFFFF)) ?
			"matched" : "not matched");

		if (crc != fw->ap.checksum)
			return -EFAULT;
	}

	return 0;
}

static int ilitek_update_BL_v1_6(struct ilitek_fw_handle *fw)
{
	int error;
	struct ilitek_ts_device *dev = fw->dev;
	struct ilitek_ts_callback *cb = &dev->cb;
	unsigned int i, j;

	uint32_t checksum;
	uint8_t total_checksum = 0;
	uint32_t df_checksum = 0, ap_checksum = 0;

	uint32_t tmp;

	if ((error = api_write_enable_v3(dev, false, false, fw->df.end,
					 fw->df.checksum)) < 0)
		return error;

	cb->delay_ms(20);

	TP_MSG("Start to write DF data...\n");
	memset(dev->wbuf, 0, sizeof(dev->wbuf));
	for (i = fw->df.start, j = 0; i < fw->df.end; i += 32) {
		memcpy(dev->wbuf + 1, fw->fw_buf + i, 32);
		if ((error = api_write_data_v3(dev)) < 0)
			return error;

		checksum = get_checksum(0, 32, dev->wbuf + 1,
					sizeof(dev->wbuf) - 1);
		df_checksum += checksum;
		total_checksum += checksum;

		if (!(j % 16))
			cb->delay_ms(50);
		else
			cb->delay_ms(3);
		j++;

		fw->progress_curr = MIN(fw->progress_curr + 32,
					fw->progress_max);
		fw->progress = (100 * fw->progress_curr) / fw->progress_max;
		TP_DBG("DF update progress: %hhu%%", fw->progress);

		if (fw->cb.update_progress)
			fw->cb.update_progress(fw->progress, fw->_private);
	}

	/* BL 1.6 need more 32 byte 0xFF, end addr need to be multiples of 32 */
	tmp = ((fw->ap.end + 1) % 32) ? 32 + 32 - ((fw->ap.end + 1) % 32) : 32;
	fw->ap.end += tmp;
	fw->ap.checksum += (tmp * 0xff);

	if ((error = api_write_enable_v3(dev, false, true, fw->ap.end,
					 fw->ap.checksum)) < 0)
		return error;

	TP_MSG("(after modified) AP start/end: %#x/%#x, checksum: %#x\n",
		fw->ap.start, fw->ap.end, fw->ap.checksum);

	cb->delay_ms(30);

	TP_MSG("Start to write AP data...\n");
	memset(dev->wbuf, 0, sizeof(dev->wbuf));
	for (i = fw->ap.start, j = 0; i < fw->ap.end; i += 32) {
		memcpy(dev->wbuf + 1, fw->fw_buf + i, 32);

		//TODO: [Joe] check if below condition check is required
		if (i + 32 > fw->ap.end)
			dev->wbuf[32] = 0;

		checksum = get_checksum(0, 32, dev->wbuf + 1,
					sizeof(dev->wbuf) - 1);
		ap_checksum += checksum;
		total_checksum += checksum;

		if ((error = api_write_data_v3(dev)) < 0)
			return error;

		if (!(j % 16))
			cb->delay_ms(50);
		else
			cb->delay_ms(3);
		j++;

		fw->progress_curr = MIN(fw->progress_curr + 32,
					fw->progress_max);
		fw->progress = (100 * fw->progress_curr) / fw->progress_max;
		TP_DBG("AP update progress: %hhu%%", fw->progress);

		if (fw->cb.update_progress)
			fw->cb.update_progress(fw->progress, fw->_private);
	}

	dev->wbuf[0] = CMD_GET_AP_CRC;
	if ((error = cb->write_then_read(dev->wbuf, 1, dev->rbuf, 1,
					 dev->_private)) < 0)
		return error;

	TP_MSG("AP checksum (file/write): %#x/%#x, DF checksum (file/write): %#x/%#x\n",
		fw->ap.checksum, ap_checksum,
		fw->df.checksum, df_checksum);
	TP_MSG("Total checksum fw:%hhx, write:%hhx\n",
		dev->rbuf[0], total_checksum);

#ifdef ILITEK_BOOT_UPDATE
	/*
	 * .ili file fill DF section with default 0xFF, but not 0
	 * which make flow stuck in BL mode.
	 * so HW-reset is need before switching to AP mode.
	 * remove HW-reset after fixing .ili converter bug.
	 */
	reset_helper(dev);
#endif
	return 0;
}

static void update_progress(struct ilitek_fw_handle *fw)
{
	struct ilitek_ts_device *dev = fw->dev;
	uint8_t i;
	unsigned int last_end = 0, last_offset = 0;

	fw->progress = 0;
	fw->progress_max = 0;
	fw->progress_curr = 0;

	switch (dev->protocol.flag) {
	case PTL_V3:
		if (fw->df.end > fw->df.start)
			fw->progress_max += fw->df.end - fw->df.start;
		if (fw->ap.end > fw->ap.start)
			fw->progress_max += fw->ap.end - fw->ap.start;
		break;

	case PTL_V6:
		for (i = 0; i < fw->block_num; i++) {
			if (fw->blocks[i].crc_match)
				continue;

			fw->progress_max +=
				fw->blocks[i].end - fw->blocks[i].start;
			last_offset += fw->blocks[i].start - last_end;
			fw->blocks[i].offset = last_offset;

			last_end = fw->blocks[i].end;
		}

		if (fw->m2v_need_update)
			fw->progress_max += fw->ap.end - fw->ap.start;

		break;
	}
}

void *ilitek_update_init(void *_dev, struct ilitek_update_callback *cb,
			 void *_private)
{
	struct ilitek_fw_handle *fw;
	struct ilitek_ts_device *dev = (struct ilitek_ts_device *)_dev;

	TP_MSG("CommonFlow-Update code version: %#x\n", UPDATE_CODE_VERSION);

	if (!dev)
		return NULL;

	if (api_update_ts_info(dev) < 0)
		return NULL;

	fw = (struct ilitek_fw_handle *)MALLOC(sizeof(*fw));
	if (!fw)
		return NULL;

	/* initial all member to 0/ false/ NULL */
	memset(fw, 0, sizeof(*fw));

	fw->dev = dev;
	fw->_private = _private;
	fw->fw_buf = (uint8_t *)CALLOC(ILITEK_FW_BUF_SIZE, 1);
	if (!fw->fw_buf)
		goto err_free_fw;

	fw->m2v_buf = (uint8_t *)CALLOC(ILITEK_FW_BUF_SIZE, 1);
	if (!fw->m2v_buf)
		goto err_free_fw_buf;

	if (cb)
		memcpy(&fw->cb, cb, sizeof(struct ilitek_update_callback));

	if (fw->cb.update_info) {
		g_update_info = fw->cb.update_info;
		g_private = fw->_private;
	}

	return fw;

err_free_fw_buf:
	CFREE(fw->fw_buf);
err_free_fw:
	FREE(fw);

	return NULL;
}

void ilitek_update_exit(void *handle)
{
	struct ilitek_fw_handle *fw = (struct ilitek_fw_handle *)handle;

	if (!handle)
		return;

	if (fw->fw_buf)
		CFREE(fw->fw_buf);

	if (fw->m2v_buf)
		CFREE(fw->m2v_buf);

	if (fw)
		FREE(fw);
}

int ilitek_update_load_fw(void *handle, char *fw_name)
{
	int error;
	struct ilitek_fw_handle *fw = (struct ilitek_fw_handle *)handle;
	struct ilitek_ts_device *dev;

	if (!handle)
		return -EINVAL;
	dev = fw->dev;

	/* buffer initialization */
	memset(fw->fw_buf, 0xFF, ILITEK_FW_BUF_SIZE);
	if (dev->mcu.ic_name == 0x2312 || dev->mcu.ic_name == 0x2315)
		memset(fw->fw_buf + 0x1F000, 0, 0x1000);
	memset(fw->m2v_buf, 0xFF, ILITEK_FW_BUF_SIZE);

	if ((error = decode_firmware(fw, fw_name)) < 0)
		return error;

	if (dev->protocol.flag == PTL_V6) {
		fw->file_fw_ver[4] =
			fw->fw_buf[dev->mcu.df_start_addr + 7];
		fw->file_fw_ver[5] =
			fw->fw_buf[dev->mcu.df_start_addr + 6];
		fw->file_fw_ver[6] =
			fw->fw_buf[dev->mcu.df_start_addr + 5];
		fw->file_fw_ver[7] =
			fw->fw_buf[dev->mcu.df_start_addr + 4];

		UPDATE_UI_INFO("File FW version: %02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X\n",
			       fw->file_fw_ver[0], fw->file_fw_ver[1],
			       fw->file_fw_ver[2], fw->file_fw_ver[3],
			       fw->file_fw_ver[4], fw->file_fw_ver[5],
			       fw->file_fw_ver[6], fw->file_fw_ver[7]);
	}

	if ((error = api_set_ctrl_mode(dev, mode_suspend, false)) < 0)
		return error;

	need_fw_update(fw);

	if ((error = api_set_ctrl_mode(dev, mode_normal, false)) < 0)
		return error;;

	return 0;
}

int ilitek_update_start(void *handle)
{
	int error;
	int8_t retry = 0;
	struct ilitek_fw_handle *fw = (struct ilitek_fw_handle *)handle;
	struct ilitek_ts_device *dev;

	if (!handle)
		return -EINVAL;
	dev = fw->dev;

	do {
		TP_MSG("fw update start, retry: %hhd, retry_limit: %hhd\n",
			retry, fw->setting.retry);
		if (retry)
			reset_helper(dev);

		if ((error = api_set_ctrl_mode(dev, mode_suspend, false)) < 0)
			continue;

		if (!need_fw_update(fw))
			goto success_return;

		update_progress(fw);
		if (fw->cb.update_progress)
			fw->cb.update_progress(0, fw->_private);

		if ((error = api_to_bl_mode(dev, true, 0, 0)) < 0)
			continue;

		if (api_protocol_set_cmd(dev, GET_PTL_VER, NULL) < 0)
			continue;

		switch (dev->protocol.ver & 0xFFFF00) {
		case BL_PROTOCOL_V1_8:
			error = ilitek_update_BL_v1_8(fw);
			break;
		case BL_PROTOCOL_V1_7:
			error = ilitek_update_BL_v1_7(fw);
			break;
		case BL_PROTOCOL_V1_6:
			error = ilitek_update_BL_v1_6(fw);
			break;
		default:
			TP_ERR("BL protocol ver: 0x%x not supported\n",
				dev->protocol.ver);
			continue;
		}
		if (error < 0)
			continue;

		if ((error = api_to_bl_mode(dev, false, fw->blocks[0].start,
					    fw->blocks[0].end)) < 0)
			continue;

success_return:
		/*
		 * After change to AP mode, V3 need to erase data flash
		 * if no data flash section in firmware file
		 */
		if (dev->protocol.flag == PTL_V3 &&
		    fw->df.end < fw->df.start &&
		    (error = api_erase_data_v3(dev)) < 0)
			continue;

		if ((error = api_update_ts_info(dev)) < 0)
			continue;

		if ((error = api_set_ctrl_mode(dev, mode_normal, false)) < 0)
			continue;

		if (fw->cb.update_progress)
			fw->cb.update_progress(100, fw->_private);

		TP_MSG("[ilitek_update_start] fw update success\n");

		return 0;
	} while (++retry < fw->setting.retry);

	TP_ERR("[ilitek_update_start] fw update failed, err: %d\n", error);

	return (error < 0) ? error : -EFAULT;
}

void ilitek_update_setting(void *handle, struct ilitek_fw_settings *setting)
{
	struct ilitek_fw_handle *fw = (struct ilitek_fw_handle *)handle;

	if (!handle)
		return;

	memcpy(&fw->setting, setting, sizeof(struct ilitek_fw_settings));
}

