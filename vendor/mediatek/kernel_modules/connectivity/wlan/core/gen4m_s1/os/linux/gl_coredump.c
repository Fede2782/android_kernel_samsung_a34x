/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "gl_coredump.h"
#if CFG_SUPPORT_CONNINFRA
#include "connsys_debug_utility.h"
#endif

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
#if CFG_SUPPORT_CONNINFRA
static int coredump_check_reg_readable(void);
#endif

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
struct coredump_ctx g_coredump_ctx;

#if CFG_SUPPORT_CONNINFRA
struct coredump_event_cb g_wifi_coredump_cb = {
	.reg_readable = coredump_check_reg_readable,
	.poll_cpupcr = NULL,
};
#endif

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#if CFG_SUPPORT_CONNINFRA
static int coredump_check_reg_readable(void)
{
	struct coredump_ctx *ctx = &g_coredump_ctx;
	int ret = 1;

	if (conninfra_reg_readable() == 0) {
		DBGLOG(INIT, ERROR,
			"conninfra_reg_readable check failed.\n");
		ret = 0;
	}

	if (ctx->fn_check_bus_hang) {
		if (ctx->fn_check_bus_hang(NULL, 0) != 0) {
			DBGLOG(INIT, ERROR, "bus check failed.\n");
			ret = 0;
		}
	}

	return ret;
}
#endif

void coredump_register_bushang_chk_cb(bushang_chk_func_cb cb)
{
	struct coredump_ctx *ctx = &g_coredump_ctx;

	ctx->fn_check_bus_hang = cb;
}

int wifi_coredump_init(void *priv)
{
	struct coredump_ctx *ctx = &g_coredump_ctx;
	struct mt66xx_chip_info *chip_info;
	int ret = 0;

	kalMemZero(ctx, sizeof(*ctx));

	ctx->priv = priv;
	glGetChipInfo((void **)&chip_info);
	if (!chip_info) {
		DBGLOG(INIT, ERROR, "chip info is NULL\n");
		ret = -EINVAL;
		goto exit;
	}

#if CFG_SUPPORT_CONNINFRA
	ctx->handler = connsys_coredump_init(CONN_DEBUG_TYPE_WIFI,
		&g_wifi_coredump_cb);
	if (!ctx->handler) {
		DBGLOG(INIT, ERROR, "connsys_coredump_init init failed.\n");
		ret = -EINVAL;
		goto exit;
	}
#endif

	ctx->initialized = TRUE;

exit:
	return ret;
}

void wifi_coredump_deinit(void)
{
	struct coredump_ctx *ctx = &g_coredump_ctx;

	ctx->initialized = FALSE;

#if CFG_SUPPORT_CONNINFRA
	connsys_coredump_deinit(ctx->handler);
#endif

	ctx->handler = NULL;
	ctx->fn_check_bus_hang = NULL;
	ctx->priv = NULL;
}

void wifi_coredump_start(enum COREDUMP_SOURCE_TYPE source,
	char *reason,
	u_int8_t force_dump)
{
	struct coredump_ctx *ctx = &g_coredump_ctx;

	if (!ctx->initialized) {
		DBGLOG(INIT, WARN,
			"Skip coredump due to NOT initialized.\n");
		return;
	}

	DBGLOG(INIT, INFO, "source: %d, reason: %s, force_dump: %d\n",
		source, reason, force_dump);
	ctx->processing = TRUE;

#if CFG_SUPPORT_CONNINFRA
	{
		enum consys_drv_type drv_type;

		drv_type = coredump_src_to_conn_type(source);
		connsys_coredump_start(ctx->handler, 0,
			drv_type, reason);
		connsys_coredump_clean(ctx->handler);
	}
#endif
	ctx->processing = FALSE;
}

#if CFG_SUPPORT_CONNINFRA
enum consys_drv_type coredump_src_to_conn_type(enum COREDUMP_SOURCE_TYPE src)
{
	enum consys_drv_type drv_type;

	switch (src) {
	case COREDUMP_SOURCE_WF_FW:
		drv_type = CONNDRV_TYPE_MAX;
		break;
	case COREDUMP_SOURCE_BT:
		drv_type = CONNDRV_TYPE_BT;
		break;
	case COREDUMP_SOURCE_FM:
		drv_type = CONNDRV_TYPE_FM;
		break;
	case COREDUMP_SOURCE_GPS:
		drv_type = CONNDRV_TYPE_GPS;
		break;
	case COREDUMP_SOURCE_CONNINFRA:
		drv_type = CONNDRV_TYPE_CONNINFRA;
		break;
	case COREDUMP_SOURCE_WF_DRIVER:
	default:
		drv_type = CONNDRV_TYPE_WIFI;
		break;
	}

	return drv_type;
}

enum COREDUMP_SOURCE_TYPE coredump_conn_type_to_src(enum consys_drv_type src)
{
	enum COREDUMP_SOURCE_TYPE type;

	switch (src) {
	case CONNDRV_TYPE_BT:
		type = COREDUMP_SOURCE_BT;
		break;
	case CONNDRV_TYPE_FM:
		type = COREDUMP_SOURCE_FM;
		break;
	case CONNDRV_TYPE_GPS:
		type = COREDUMP_SOURCE_GPS;
		break;
	case CONNDRV_TYPE_CONNINFRA:
		type = COREDUMP_SOURCE_CONNINFRA;
		break;
	case CONNDRV_TYPE_WIFI:
	default:
		type = COREDUMP_SOURCE_WF_DRIVER;
		break;
	}

	return type;
}
#endif

void wifi_coredump_set_enable(u_int8_t enable)
{
	struct coredump_ctx *ctx = &g_coredump_ctx;

	ctx->enable = enable;
}

u_int8_t is_wifi_coredump_processing(void)
{
	struct coredump_ctx *ctx = &g_coredump_ctx;

	return ctx->processing;
}

