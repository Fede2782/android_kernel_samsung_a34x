/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef _FIVE_INTEGRITY_H
#define _FIVE_INTEGRITY_H

#include "security/integrity/integrity.h"
#include "five_porting.h"

struct five_iint_cache {
	struct rb_node rb_node;	/* rooted in integrity_iint_tree */
	struct mutex mutex;	/* protects: version, flags, digest */
	struct inode *inode;	/* back pointer to inode in question */
	u64 version;		/* track inode changes */
	unsigned long flags;
	unsigned long measured_pcrs;
	unsigned long atomic_flags;
	unsigned long real_ino;
	dev_t real_dev;
	unsigned long five_flags;
	enum five_file_integrity five_status;
	struct integrity_label *five_label;
	bool five_signing;
};

struct five_iint_cache *five_iint_find(struct inode *inode);
struct five_iint_cache *five_inode_get(struct inode *inode);
void five_inode_free(struct inode *inode);
int five_iintcache_init(void);

#endif /* _FIVE_INTEGRITY_H */
