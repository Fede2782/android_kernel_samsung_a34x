// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2008 IBM Corporation
 *
 * Authors:
 * Mimi Zohar <zohar@us.ibm.com>
 *
 * File: five_iint.c
 *	- implements the integrity hooks: integrity_inode_alloc,
 *	  integrity_inode_free
 *	- cache integrity information associated with an inode
 *	  using a rbtree tree.
 */
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/rbtree.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/security.h>
#include <uapi/linux/magic.h>
#include "five_iint.h"
#include "five_log.h"

static struct rb_root five_iint_tree = RB_ROOT;
static DEFINE_RWLOCK(five_iint_lock);
static struct kmem_cache *five_iint_cache __read_mostly;

/*
 * __five_iint_find - return the iint associated with an inode
 */
static struct five_iint_cache *__five_iint_find(struct inode *inode)
{
	struct five_iint_cache *iint;
	struct rb_node *n = five_iint_tree.rb_node;

	while (n) {
		iint = rb_entry(n, struct five_iint_cache, rb_node);

		if (inode < iint->inode)
			n = n->rb_left;
		else if (inode > iint->inode)
			n = n->rb_right;
		else
			return iint;
	}

	return NULL;
}

struct five_iint_cache *five_iint_find(struct inode *inode)
{
	struct five_iint_cache *iint;

	read_lock(&five_iint_lock);
	iint = __five_iint_find(inode);
	read_unlock(&five_iint_lock);

	return iint;
}

static void five_iint_init_always(struct five_iint_cache *iint,
			     struct inode *inode)
{
	iint->version = 0;
	iint->flags = 0UL;
	iint->atomic_flags = 0UL;
	iint->measured_pcrs = 0;
	mutex_init(&iint->mutex);
}

static void five_iint_free(struct five_iint_cache *iint)
{
	kfree(iint->five_label);
	iint->five_label = NULL;
	iint->five_flags = 0UL;
	iint->five_status = FIVE_FILE_UNKNOWN;
	iint->five_signing = false;
	mutex_destroy(&iint->mutex);
	kmem_cache_free(five_iint_cache, iint);
}

/**
 * five_inode_get - find or allocate an iint associated with an inode
 * @inode: pointer to the inode
 * @return: allocated iint
 *
 * Caller must lock i_mutex
 */
struct five_iint_cache *five_inode_get(struct inode *inode)
{
	struct rb_node **p;
	struct rb_node *node, *parent = NULL;
	struct five_iint_cache *iint, *test_iint;

	if (!five_iint_cache) {
#ifdef CONFIG_FIVE_DEBUG
		panic("%s: five_iint_cache fail\n", __func__);
#else
		FIVE_ERROR_LOG("five_iint_cache fail");
		return NULL;
#endif
	}

	iint = five_iint_find(inode);
	if (iint)
		return iint;

	iint = kmem_cache_alloc(five_iint_cache, GFP_NOFS);
	if (!iint)
		return NULL;

	five_iint_init_always(iint, inode);

	write_lock(&five_iint_lock);

	p = &five_iint_tree.rb_node;
	while (*p) {
		parent = *p;
		test_iint = rb_entry(parent, struct five_iint_cache,
				     rb_node);
		if (inode < test_iint->inode) {
			p = &(*p)->rb_left;
		} else if (inode > test_iint->inode) {
			p = &(*p)->rb_right;
		} else {
			write_unlock(&five_iint_lock);
			kmem_cache_free(five_iint_cache, iint);
			return test_iint;
		}
	}

	iint->inode = inode;
	node = &iint->rb_node;
	rb_link_node(node, parent, p);
	rb_insert_color(node, &five_iint_tree);

	write_unlock(&five_iint_lock);
	return iint;
}

/**
 * five_inode_free - called on security_inode_free
 * @inode: pointer to the inode
 *
 * Free the integrity information(iint) associated with an inode.
 */
void five_inode_free(struct inode *inode)
{
	struct five_iint_cache *iint;

	write_lock(&five_iint_lock);
	iint = __five_iint_find(inode);
	if (!iint) {
		write_unlock(&five_iint_lock);
		return;
	}
	rb_erase(&iint->rb_node, &five_iint_tree);
	write_unlock(&five_iint_lock);

	five_iint_free(iint);
}

static void five_iint_init_once(void *foo)
{
	struct five_iint_cache *iint = (struct five_iint_cache *) foo;

	memset(iint, 0, sizeof(*iint));
	iint->five_flags = 0UL;
	iint->five_status = FIVE_FILE_UNKNOWN;
	iint->five_signing = false;
}

int __init five_iintcache_init(void)
{
	five_iint_cache =
	    kmem_cache_create("five_iint_cache", sizeof(struct five_iint_cache),
			      0, SLAB_PANIC, five_iint_init_once);

	if (!five_iint_cache) {
		FIVE_ERROR_LOG("five_iint_cache kmem fail");
		return -1;
	}

	return 0;
}
