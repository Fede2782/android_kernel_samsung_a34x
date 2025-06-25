// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 *
 * DMA BUF page pool system
 *
 */

#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/sched/signal.h>
#include "mtk_page_pool.h"

static LIST_HEAD(pool_list);
static DEFINE_MUTEX(pool_list_lock);

static inline
struct page *mtk_dmabuf_page_pool_alloc_pages(struct mtk_dmabuf_page_pool *pool)
{
	if (fatal_signal_pending(current))
		return NULL;

	return alloc_pages(pool->gfp_mask, pool->order);
}

static inline void mtk_dmabuf_page_pool_free_pages(struct mtk_dmabuf_page_pool *pool,
					       struct page *page)
{
	__free_pages(page, pool->order);
}

void mtk_dmabuf_page_pool_add(struct mtk_dmabuf_page_pool *pool, struct page *page)
{
	int index;

	if (PageHighMem(page))
		index = POOL_HIGHPAGE;
	else
		index = POOL_LOWPAGE;

	spin_lock(&pool->lock);
	list_add_tail(&page->lru, &pool->items[index]);
	pool->count[index]++;
	spin_unlock(&pool->lock);
	mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
			    1 << pool->order);
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_add);

static struct page *mtk_dmabuf_page_pool_remove(struct mtk_dmabuf_page_pool *pool, int index)
{
	struct page *page;

	spin_lock(&pool->lock);
	page = list_first_entry_or_null(&pool->items[index], struct page, lru);
	if (page) {
		pool->count[index]--;
		list_del(&page->lru);
		spin_unlock(&pool->lock);
		mod_node_page_state(page_pgdat(page), NR_KERNEL_MISC_RECLAIMABLE,
				    -(1 << pool->order));
		goto out;
	}
	spin_unlock(&pool->lock);
out:
	return page;
}

struct page *mtk_dmabuf_page_pool_fetch(struct mtk_dmabuf_page_pool *pool)
{
	struct page *page = NULL;

	page = mtk_dmabuf_page_pool_remove(pool, POOL_HIGHPAGE);
	if (!page)
		page = mtk_dmabuf_page_pool_remove(pool, POOL_LOWPAGE);

	return page;
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_fetch);

struct page *mtk_dmabuf_page_pool_alloc(struct mtk_dmabuf_page_pool *pool)
{
	struct page *page = NULL;

	if (WARN_ON(!pool))
		return NULL;

	page = mtk_dmabuf_page_pool_fetch(pool);
	if (!page)
		page = mtk_dmabuf_page_pool_alloc_pages(pool);
	return page;
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_alloc);

void mtk_dmabuf_page_pool_free(struct mtk_dmabuf_page_pool *pool, struct page *page)
{
	if (WARN_ON(pool->order != compound_order(page)))
		return;

	mtk_dmabuf_page_pool_add(pool, page);
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_free);

int mtk_dmabuf_page_pool_total(struct mtk_dmabuf_page_pool *pool, bool high)
{
	int count;

	spin_lock(&pool->lock);
	count = pool->count[POOL_LOWPAGE];
	if (high)
		count += pool->count[POOL_HIGHPAGE];
	spin_unlock(&pool->lock);

	return count << pool->order;
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_total);

long mtk_dmabuf_page_pool_size(struct dma_heap *heap)
{
	struct mtk_heap_priv_info *heap_priv;
	struct mtk_dmabuf_page_pool **pools;
	long num_pages = 0, i;

	heap_priv = dma_heap_get_drvdata(heap);
	pools = heap_priv ? heap_priv->page_pools : NULL;
	if (!pools)
		return 0;

	for (i = 0; i < NUM_ORDERS; i++)
		num_pages += mtk_dmabuf_page_pool_total(pools[i], true);

	return num_pages << PAGE_SHIFT;
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_size);

struct mtk_dmabuf_page_pool *mtk_dmabuf_page_pool_create(gfp_t gfp_mask, unsigned int order)
{
	struct mtk_dmabuf_page_pool *pool = kzalloc(sizeof(*pool), GFP_KERNEL);
	int i;

	if (!pool)
		return NULL;

	for (i = 0; i < POOL_TYPE_SIZE; i++) {
		pool->count[i] = 0;
		INIT_LIST_HEAD(&pool->items[i]);
	}
	pool->gfp_mask = gfp_mask | __GFP_COMP;
	pool->order = order;
	spin_lock_init(&pool->lock);

	mutex_lock(&pool_list_lock);
	list_add(&pool->list, &pool_list);
	mutex_unlock(&pool_list_lock);

	return pool;
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_create);

void mtk_dmabuf_page_pool_destroy(struct mtk_dmabuf_page_pool *pool)
{
	struct page *page;
	int i;

	/* Remove us from the pool list */
	mutex_lock(&pool_list_lock);
	list_del(&pool->list);
	mutex_unlock(&pool_list_lock);

	/* Free any remaining pages in the pool */
	for (i = 0; i < POOL_TYPE_SIZE; i++) {
		while ((page = mtk_dmabuf_page_pool_remove(pool, i)))
			mtk_dmabuf_page_pool_free_pages(pool, page);
	}

	kfree(pool);
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_destroy);

unsigned long mtk_dmabuf_page_pool_get_size(struct mtk_dmabuf_page_pool *pool)
{
	int i;
	unsigned long num_pages = 0;

	spin_lock(&pool->lock);
	for (i = 0; i < POOL_TYPE_SIZE; ++i)
		num_pages += pool->count[i];
	spin_unlock(&pool->lock);
	num_pages <<= pool->order; /* pool order is immutable */

	return num_pages * PAGE_SIZE;
}
EXPORT_SYMBOL_GPL(mtk_dmabuf_page_pool_get_size);

int mtk_dmabuf_page_pool_do_shrink(struct mtk_dmabuf_page_pool *pool, gfp_t gfp_mask,
				      int nr_to_scan)
{
	int freed = 0;
	bool high;

	if (current_is_kswapd())
		high = true;
	else
		high = !!(gfp_mask & __GFP_HIGHMEM);

	if (nr_to_scan == 0)
		return mtk_dmabuf_page_pool_total(pool, high);

	while (freed < nr_to_scan) {
		struct page *page;

		/* Try to free low pages first */
		page = mtk_dmabuf_page_pool_remove(pool, POOL_LOWPAGE);
		if (!page)
			page = mtk_dmabuf_page_pool_remove(pool, POOL_HIGHPAGE);

		if (!page)
			break;

		mtk_dmabuf_page_pool_free_pages(pool, page);
		freed += (1 << pool->order);
	}

	return freed;
}

static int mtk_dmabuf_page_pool_shrink(gfp_t gfp_mask, int nr_to_scan)
{
	struct mtk_dmabuf_page_pool *pool;
	int nr_total = 0;
	int nr_freed;
	int only_scan = 0;

	if (!nr_to_scan)
		only_scan = 1;

	mutex_lock(&pool_list_lock);
	list_for_each_entry(pool, &pool_list, list) {
		if (only_scan) {
			nr_total += mtk_dmabuf_page_pool_do_shrink(pool,
							       gfp_mask,
							       nr_to_scan);
		} else {
			nr_freed = mtk_dmabuf_page_pool_do_shrink(pool,
							      gfp_mask,
							      nr_to_scan);
			nr_to_scan -= nr_freed;
			nr_total += nr_freed;
			if (nr_to_scan <= 0)
				break;
		}
	}
	mutex_unlock(&pool_list_lock);

	return nr_total;
}

static unsigned long mtk_dmabuf_page_pool_shrink_count(struct shrinker *shrinker,
						   struct shrink_control *sc)
{
	return mtk_dmabuf_page_pool_shrink(sc->gfp_mask, 0);
}

static unsigned long mtk_dmabuf_page_pool_shrink_scan(struct shrinker *shrinker,
						  struct shrink_control *sc)
{
	if (sc->nr_to_scan == 0)
		return 0;
	return mtk_dmabuf_page_pool_shrink(sc->gfp_mask, sc->nr_to_scan);
}

static struct shrinker mtk_pool_shrinker = {
	.count_objects = mtk_dmabuf_page_pool_shrink_count,
	.scan_objects = mtk_dmabuf_page_pool_shrink_scan,
	.seeks = 16,
	.batch = 0,
};

int mtk_dmabuf_page_pool_init_shrinker(void)
{
	return register_shrinker(&mtk_pool_shrinker, "");
}

struct mtk_dmabuf_page_pool **dynamic_page_pools_create(unsigned int *orders,
							unsigned int num_orders)
{
	struct mtk_dmabuf_page_pool **pool_list;
	int i;
	int ret;

	pool_list = kmalloc_array(num_orders, sizeof(*pool_list), GFP_KERNEL);
	if (!pool_list)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < num_orders; i++) {
		pool_list[i] = mtk_dmabuf_page_pool_create(order_flags[i], orders[i]);

		if (IS_ERR(pool_list[i])) {
			int j;

			pr_err("%s: page pool creation failed for order %d!\n",
				__func__, orders[i]);
			for (j = 0; j < i; j++)
				mtk_dmabuf_page_pool_destroy(pool_list[j]);
			ret = -ENOMEM;
			goto free_pool_list;
		}

		pool_list[i]->order_index = i;
		pool_list[i]->pool_list = pool_list;
	}
	return pool_list;

free_pool_list:
	kfree(pool_list);
	return ERR_PTR(ret);
}

void dynamic_page_pools_free(struct mtk_dmabuf_page_pool **pool_list, unsigned int num_orders)
{
	int i;

	if (!pool_list)
		return;

	for (i = 0; i < num_orders; i++)
		mtk_dmabuf_page_pool_destroy(pool_list[i]);

	kfree(pool_list);
}
