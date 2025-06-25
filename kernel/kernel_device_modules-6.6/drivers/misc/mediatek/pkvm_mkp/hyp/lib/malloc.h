/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MALLOC_H
#define __MALLOC_H

#include <asm/kvm_pkvm_module.h>

int malloc_init(struct pkvm_module_ops *ops, u64 heap_start, u64 heap_size);
void *malloc(size_t nr_bytes);
void free(void *ptr);

#endif
