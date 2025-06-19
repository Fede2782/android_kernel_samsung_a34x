/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __MTK_DCM_AUTOGEN_H__
#define __MTK_DCM_AUTOGEN_H__

#include <mtk_dcm.h>

#if IS_ENABLED(CONFIG_OF)
/* TODO: Fix all base addresses. */
extern unsigned long dcm_mcusys_par_wrap_base;
extern unsigned long dcm_bcrm_apinfra_io_ctrl_ao;
extern unsigned long dcm_bcrm_apinfra_io_noc_ao;
extern unsigned long dcm_bcrm_apinfra_mem_intf_noc_ao;
extern unsigned long dcm_bcrm_apinfra_mem_ctrl_ao;
extern unsigned long dcm_peri_ao_bcrm_base;
extern unsigned long dcm_vlp_ao_bcrm_base;

#if !defined(MCUSYS_PAR_WRAP_BASE)
#define MCUSYS_PAR_WRAP_BASE (dcm_mcusys_par_wrap_base)
#endif /* !defined(MCUSYS_PAR_WRAP_BASE) */
#if !defined(bcrm_APINFRA_IO_CTRL_AO)
#define bcrm_APINFRA_IO_CTRL_AO (dcm_bcrm_apinfra_io_ctrl_ao)
#endif /* !defined(bcrm_APINFRA_IO_CTRL_AO) */
#if !defined(bcrm_APINFRA_IO_NOC_AO)
#define bcrm_APINFRA_IO_NOC_AO (dcm_bcrm_apinfra_io_noc_ao)
#endif /* !defined(bcrm_APINFRA_IO_NOC_AO) */
#if !defined(bcrm_APINFRA_MEM_INTF_NOC_AO)
#define bcrm_APINFRA_MEM_INTF_NOC_AO (dcm_bcrm_apinfra_mem_intf_noc_ao)
#endif /* !defined(bcrm_APINFRA_MEM_INTF_NOC_AO) */
#if !defined(bcrm_APINFRA_MEM_CTRL_AO)
#define bcrm_APINFRA_MEM_CTRL_AO (dcm_bcrm_apinfra_mem_ctrl_ao)
#endif /* !defined(bcrm_APINFRA_MEM_CTRL_AO) */
#if !defined(PERI_AO_BCRM_BASE)
#define PERI_AO_BCRM_BASE (dcm_peri_ao_bcrm_base)
#endif /* !defined(PERI_AO_BCRM_BASE) */
#if !defined(VLP_AO_BCRM_BASE)
#define VLP_AO_BCRM_BASE (dcm_vlp_ao_bcrm_base)
#endif /* !defined(VLP_AO_BCRM_BASE) */

#else /* !IS_ENABLED(CONFIG_OF)) */

/* Here below used in CTP and pl for references. */
#undef MCUSYS_PAR_WRAP_BASE
#undef bcrm_APINFRA_IO_CTRL_AO
#undef bcrm_APINFRA_IO_NOC_AO
#undef bcrm_APINFRA_MEM_INTF_NOC_AO
#undef bcrm_APINFRA_MEM_CTRL_AO
#undef PERI_AO_BCRM_BASE
#undef VLP_AO_BCRM_BASE

/* Base */
#define MCUSYS_PAR_WRAP_BASE         0xc1b0000
#define bcrm_APINFRA_IO_CTRL_AO      0x10156000
#define bcrm_APINFRA_IO_NOC_AO       0x14012000
#define bcrm_APINFRA_MEM_INTF_NOC_AO 0x14032000
#define bcrm_APINFRA_MEM_CTRL_AO     0x14124000
#define PERI_AO_BCRM_BASE            0x16610000
#define VLP_AO_BCRM_BASE             0x1c030000
#endif /* if IS_ENABLED(CONFIG_OF)) */

/* Register Definition */
#define MCUSYS_PAR_WRAP_NON_SEC_L3_SHARE_DCM_CTRL                                        (MCUSYS_PAR_WRAP_BASE + 0x078)
#define MCUSYS_PAR_WRAP_NON_SEC_MP_ADB_DCM_CFG0                                          (MCUSYS_PAR_WRAP_BASE + 0x270)
#define MCUSYS_PAR_WRAP_NON_SEC_ADB_FIFO_DCM_EN                                          (MCUSYS_PAR_WRAP_BASE + 0x278)
#define MCUSYS_PAR_WRAP_NON_SEC_MP0_DCM_CFG0                                             (MCUSYS_PAR_WRAP_BASE + 0x27c)
#define MCUSYS_PAR_WRAP_NON_SEC_QDCM_CONFIG0                                             (MCUSYS_PAR_WRAP_BASE + 0x280)
#define MCUSYS_PAR_WRAP_NON_SEC_L3GIC_ARCH_CG_CONFIG                                     (MCUSYS_PAR_WRAP_BASE + 0x294)
#define MCUSYS_PAR_WRAP_NON_SEC_QDCM_CONFIG1                                             (MCUSYS_PAR_WRAP_BASE + 0x284)
#define MCUSYS_PAR_WRAP_NON_SEC_QDCM_CONFIG2                                             (MCUSYS_PAR_WRAP_BASE + 0x288)
#define MCUSYS_PAR_WRAP_NON_SEC_QDCM_CONFIG3                                             (MCUSYS_PAR_WRAP_BASE + 0x28c)
#define MCUSYS_PAR_WRAP_NON_SEC_CI700_DCM_CTRL                                           (MCUSYS_PAR_WRAP_BASE + 0x298)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_CABGEN_3TO1_CONFIG                                  (MCUSYS_PAR_WRAP_BASE + 0x2a0)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_CABGEN_2TO1_CONFIG                                  (MCUSYS_PAR_WRAP_BASE + 0x2a4)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_CABGEN_4TO2_CONFIG                                  (MCUSYS_PAR_WRAP_BASE + 0x2a8)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_CABGEN_1TO2_CONFIG                                  (MCUSYS_PAR_WRAP_BASE + 0x2ac)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_CABGEN_2TO5_CONFIG                                  (MCUSYS_PAR_WRAP_BASE + 0x2b0)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_P2P_CONFIG0                                         (MCUSYS_PAR_WRAP_BASE + 0x2b4)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_CABGEN_1TO2_L3GIC_CONFIG                            (MCUSYS_PAR_WRAP_BASE + 0x2bc)
#define MCUSYS_PAR_WRAP_NON_SEC_CBIP_CABGEN_1TO2_INFRA_CONFIG                            (MCUSYS_PAR_WRAP_BASE + 0x2c4)
#define MCUSYS_PAR_WRAP_NON_SEC_MP_CENTRAL_FABRIC_SUB_CHANNEL_CG                         (MCUSYS_PAR_WRAP_BASE + 0x2b8)
#define MCUSYS_PAR_WRAP_NON_SEC_ACP_SLAVE_DCM_EN                                         (MCUSYS_PAR_WRAP_BASE + 0x2dc)
#define MCUSYS_PAR_WRAP_NON_SEC_GIC_SPI_SLOW_CK_CFG                                      (MCUSYS_PAR_WRAP_BASE + 0x2e0)
#define MCUSYS_PAR_WRAP_NON_SEC_EBG_CKE_WRAP_FIFO_CFG                                    (MCUSYS_PAR_WRAP_BASE + 0x404)
#define CLK_AXI_VDNR_DCM_TOP_APINFRA_IO_INTX_BUS_u_APINFRA_IO_INTX_BUS_CTRL_0            \
								(bcrm_APINFRA_IO_CTRL_AO + 0x8)
#define CLK_IO_NOC_VDNR_DCM_TOP_APINFRA_IO_INTF_PAR_BUS_u_APINFRA_IO_INTF_PAR_BUS_CTRL_0 \
								(bcrm_APINFRA_IO_NOC_AO + 0x4)
#define VDNR_DCM_TOP_APINFRA_MEM_INTF_PAR_BUS_u_APINFRA_MEM_INTF_PAR_BUS_CTRL_0          \
								(bcrm_APINFRA_MEM_INTF_NOC_AO + 0x0)
#define CLK_FMEM_SUB_CFG_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_0 \
								(bcrm_APINFRA_MEM_CTRL_AO + 0xc)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_0     \
								(bcrm_APINFRA_MEM_CTRL_AO + 0x14)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_1     \
								(bcrm_APINFRA_MEM_CTRL_AO + 0x18)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_2     \
								(bcrm_APINFRA_MEM_CTRL_AO + 0x1c)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_3     \
								(bcrm_APINFRA_MEM_CTRL_AO + 0x20)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_4     \
								(bcrm_APINFRA_MEM_CTRL_AO + 0x24)
#define CLK_FMEM_SUB_VDNR_DCM_TOP_APINFRA_MEM_INTX_BUS_u_APINFRA_MEM_INTX_BUS_CTRL_5     \
								(bcrm_APINFRA_MEM_CTRL_AO + 0x28)
#define A0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0                               (PERI_AO_BCRM_BASE + 0x2c)
#define A0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1                               (PERI_AO_BCRM_BASE + 0x30)
#define A0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2                               (PERI_AO_BCRM_BASE + 0x34)
#define A0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_3                               (PERI_AO_BCRM_BASE + 0x38)
#define VDNR_DCM_TOP_VLP_PAR_BUS_TOP_u_VLP_PAR_BUS_TOP_CTRL_0                            (VLP_AO_BCRM_BASE + 0x5c)

/* B0 Peri register offsets */
#define B0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_0                               (PERI_AO_BCRM_BASE + 0x20)
#define B0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_1                               (PERI_AO_BCRM_BASE + 0x24)
#define B0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_2                               (PERI_AO_BCRM_BASE + 0x28)
#define B0_VDNR_DCM_TOP_PERI_PAR_BUS_u_PERI_PAR_BUS_CTRL_3                               (PERI_AO_BCRM_BASE + 0x2c)

void dcm_mcusys_par_wrap_mcu_l3c_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_l3c_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_acp_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_acp_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_adb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_adb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_stalldcm(int on);
bool dcm_mcusys_par_wrap_mcu_stalldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_apb_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_apb_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_io_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_io_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_bus_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_bus_qdcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_core_qdcm(int on);
bool dcm_mcusys_par_wrap_mcu_core_qdcm_is_on(void);
void A0_dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on);
bool A0_dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void);
void B0_dcm_mcusys_par_wrap_mcu_bkr_ldcm(int on);
bool B0_dcm_mcusys_par_wrap_mcu_bkr_ldcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_cbip_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_cbip_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_misc_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_misc_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_dsu_acp_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_dsu_acp_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_chi_mon_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_chi_mon_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_gic_spi_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_gic_spi_dcm_is_on(void);
void dcm_mcusys_par_wrap_mcu_ebg_dcm(int on);
bool dcm_mcusys_par_wrap_mcu_ebg_dcm_is_on(void);
void dcm_bcrm_apinfra_io_ctrl_ao_infra_bus_dcm(int on);
bool dcm_bcrm_apinfra_io_ctrl_ao_infra_bus_dcm_is_on(void);
void dcm_bcrm_apinfra_io_noc_ao_infra_bus_dcm(int on);
bool dcm_bcrm_apinfra_io_noc_ao_infra_bus_dcm_is_on(void);
void dcm_bcrm_apinfra_mem_intf_noc_ao_infra_bus_dcm(int on);
bool dcm_bcrm_apinfra_mem_intf_noc_ao_infra_bus_dcm_is_on(void);
void dcm_bcrm_apinfra_mem_ctrl_ao_infra_bus_dcm(int on);
bool dcm_bcrm_apinfra_mem_ctrl_ao_infra_bus_dcm_is_on(void);
void A0_dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool A0_dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void B0_dcm_peri_ao_bcrm_peri_bus_dcm(int on);
bool B0_dcm_peri_ao_bcrm_peri_bus_dcm_is_on(void);
void dcm_vlp_ao_bcrm_vlp_bus_dcm(int on);
bool dcm_vlp_ao_bcrm_vlp_bus_dcm_is_on(void);
#endif /* __MTK_DCM_AUTOGEN_H__ */
