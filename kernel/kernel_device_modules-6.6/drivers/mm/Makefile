# SPDX-License-Identifier: GPL-2.0                                                                                                                                                                                                   
#
# Makefile for the kernel block device drivers.
#
# 12 June 2000, Christoph Hellwig <hch@infradead.org>
# Rewritten to use lists instead of if-statements.
# 

# needed for trace events
ccflags-y               += -I$(src)

obj-$(CONFIG_ZSMALLOC)  += zsmalloc.o
