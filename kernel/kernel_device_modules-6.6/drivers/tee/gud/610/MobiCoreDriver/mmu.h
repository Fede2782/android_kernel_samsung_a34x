/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2023 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef MC_MMU_H
#define MC_MMU_H

#include "platform.h"

struct tee_mmu;
struct mcp_buffer_map;
struct mc_ioctl_buffer;

/*
 * Allocate MMU table and map buffer into it.
 * That is, create respective table entries.
 */
struct tee_mmu *tee_mmu_create(struct mm_struct *mm,
			       const struct mc_ioctl_buffer *buf);

/*
 * Gets a reference on a MMU table.
 */
void tee_mmu_get(struct tee_mmu *mmu);

/*
 * Puts a reference on a MMU table.
 */
void tee_mmu_put(struct tee_mmu *mmu);

/*
 * Get the MMU handle.
 */
u64 tee_mmu_get_handle(struct tee_mmu *mmu);

/*
 * Fill in buffer info for MMU table.
 */
void tee_mmu_buffer(struct tee_mmu *mmu, struct mcp_buffer_map *map);

/*
 * Add info to debug buffer.
 */
int tee_mmu_debug_structs(struct kasnprintf_buf *buf,
			  const struct tee_mmu *mmu);

#ifdef MC_SHADOW_BUFFER
/*
 * Copy forward shadow buffer from client application.
 */
int tee_mmu_copy_to_shadow(struct tee_mmu *mmu);

/*
 * Copy back shadow buffer to client application.
 */
int tee_mmu_copy_from_shadow(struct tee_mmu *mmu);

/*
 * Free allocated shadow buffer
 */
void tee_mmu_free_shadow(struct tee_mmu *mmu);

/*
 * Free allocated mmu zombie list
 */
void tee_mmu_delete_zombies(void);

/*
 * Initialize mmu mutex
 */
void mmu_init(void);
#endif /* MC_SHADOW_BUFFER */
#endif /* MC_MMU_H */
