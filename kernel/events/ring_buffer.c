/*
 * Performance events ring-buffer code:
 *
 *  Copyright (C) 2008 Thomas Gleixner <tglx@linutronix.de>
 *  Copyright (C) 2008-2011 Red Hat, Inc., Ingo Molnar
 *  Copyright (C) 2008-2011 Red Hat, Inc., Peter Zijlstra <pzijlstr@redhat.com>
 *  Copyright  ©  2009 Paul Mackerras, IBM Corp. <paulus@au1.ibm.com>
 *
 * For licensing details see kernel-base/COPYING
 */

#include <linux/perf_event.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#include "internal.h"

static bool perf_output_space(struct ring_buffer *rb, unsigned long tail,
			      unsigned long offset, unsigned long head)
{
	unsigned long mask;

	if (!rb->writable)
		return true;

	mask = perf_data_size(rb) - 1;

	offset = (offset - tail) & mask;
	head   = (head   - tail) & mask;

	if ((int)(head - offset) < 0)
		return false;

	return true;
}

static void perf_output_wakeup(struct perf_output_handle *handle)
{
	atomic_set(&handle->rb->poll, POLL_IN);

	handle->event->pending_wakeup = 1;
	irq_work_queue(&handle->event->pending);
}

/*
 * We need to ensure a later event_id doesn't publish a head when a former
 * event isn't done writing. However since we need to deal with NMIs we
 * cannot fully serialize things.
 *
 * We only publish the head (and generate a wakeup) when the outer-most
 * event completes.
 */
static void perf_output_get_handle(struct perf_output_handle *handle)
{
	struct ring_buffer *rb = handle->rb;

	preempt_disable();
	local_inc(&rb->nest);
	handle->wakeup = local_read(&rb->wakeup);
}

static void perf_output_put_handle(struct perf_output_handle *handle)
{
	struct ring_buffer *rb = handle->rb;
	unsigned long head;

again:
	head = local_read(&rb->head);

	/*
	 * IRQ/NMI can happen here and advance @rb->head, causing our
	 * load above to be stale.
	 */

	/*
	 * If this isn't the outermost nesting, we don't have to update
	 * @rb->user_page->data_head.
	 */
	if (local_read(&rb->nest) > 1) {
		local_dec(&rb->nest);
		goto out;
	}

	/*
	 * Since the mmap() consumer (userspace) can run on a different CPU:
	 *
	 *   kernel				user
	 *
	 *   READ ->data_tail			READ ->data_head
	 *   smp_mb()	(A)			smp_rmb()	(C)
	 *   WRITE $data			READ $data
	 *   smp_wmb()	(B)			smp_mb()	(D)
	 *   STORE ->data_head			WRITE ->data_tail
	 *
	 * Where A pairs with D, and B pairs with C.
	 *
	 * I don't think A needs to be a full barrier because we won't in fact
	 * write data until we see the store from userspace. So we simply don't
	 * issue the data WRITE until we observe it. Be conservative for now.
	 *
	 * OTOH, D needs to be a full barrier since it separates the data READ
	 * from the tail WRITE.
	 *
	 * For B a WMB is sufficient since it separates two WRITEs, and for C
	 * an RMB is sufficient since it separates two READs.
	 *
	 * See perf_output_begin().
	 */
	smp_wmb();
	rb->user_page->data_head = head;

	/*
	 * We must publish the head before decrementing the nest count,
	 * otherwise an IRQ/NMI can publish a more recent head value and our
	 * write will (temporarily) publish a stale value.
	 */
	barrier();
	local_set(&rb->nest, 0);

	/*
	 * Ensure we decrement @rb->nest before we validate the @rb->head.
	 * Otherwise we cannot be sure we caught the 'last' nested update.
	 */
	barrier();
	if (unlikely(head != local_read(&rb->head))) {
		local_inc(&rb->nest);
		goto again;
	}

	if (handle->wakeup != local_read(&rb->wakeup))
		perf_output_wakeup(handle);

out:
	preempt_enable();
}

int perf_output_begin(struct perf_output_handle *handle,
		      struct perf_event *event, unsigned int size)
{
	struct ring_buffer *rb;
	unsigned long tail, offset, head;
	int have_lost;
	struct perf_sample_data sample_data;
	struct {
		struct perf_event_header header;
		u64			 id;
		u64			 lost;
	} lost_event;

	rcu_read_lock();
	/*
	 * For inherited events we send all the output towards the parent.
	 */
	if (event->parent)
		event = event->parent;

	rb = rcu_dereference(event->rb);
	if (!rb)
		goto out;

	handle->rb	= rb;
	handle->event	= event;

	if (!rb->nr_pages)
		goto out;

	have_lost = local_read(&rb->lost);
	if (have_lost) {
		lost_event.header.size = sizeof(lost_event);
		perf_event_header__init_id(&lost_event.header, &sample_data,
					   event);
		size += lost_event.header.size;
	}

	perf_output_get_handle(handle);

	do {
		/*
		 * Userspace could choose to issue a mb() before updating the
		 * tail pointer. So that all reads will be completed before the
		 * write is issued.
		 *
		 * See perf_output_put_handle().
		 */
		tail = ACCESS_ONCE(rb->user_page->data_tail);
		smp_mb();
		offset = head = local_read(&rb->head);
		head += size;
		if (unlikely(!perf_output_space(rb, tail, offset, head)))
			goto fail;
	} while (local_cmpxchg(&rb->head, offset, head) != offset);

	if (head - local_read(&rb->wakeup) > rb->watermark)
		local_add(rb->watermark, &rb->wakeup);

	handle->page = offset >> (PAGE_SHIFT + page_order(rb));
	handle->page &= rb->nr_pages - 1;
	handle->size = offset & ((PAGE_SIZE << page_order(rb)) - 1);
	handle->addr = rb->data_pages[handle->page];
	handle->addr += handle->size;
	handle->size = (PAGE_SIZE << page_order(rb)) - handle->size;

	if (have_lost) {
		lost_event.header.type = PERF_RECORD_LOST;
		lost_event.header.misc = 0;
		lost_event.id          = event->id;
		lost_event.lost        = local_xchg(&rb->lost, 0);

		perf_output_put(handle, lost_event);
		perf_event__output_id_sample(event, handle, &sample_data);
	}

	return 0;

fail:
	local_inc(&rb->lost);
	perf_output_put_handle(handle);
out:
	rcu_read_unlock();

	return -ENOSPC;
}

unsigned int perf_output_copy(struct perf_output_handle *handle,
		      const void *buf, unsigned int len)
{
	return __output_copy(handle, buf, len);
}

unsigned int perf_output_skip(struct perf_output_handle *handle,
			      unsigned int len)
{
	return __output_skip(handle, NULL, len);
}

void perf_output_end(struct perf_output_handle *handle)
{
	perf_output_put_handle(handle);
	rcu_read_unlock();
}

static void
ring_buffer_init(struct ring_buffer *rb, long watermark, int flags)
{
	long max_size = perf_data_size(rb);

	if (watermark)
		rb->watermark = min(max_size, watermark);

	if (!rb->watermark)
		rb->watermark = max_size / 2;

	if (flags & RING_BUFFER_WRITABLE)
		rb->writable = 1;

	atomic_set(&rb->refcount, 1);

	INIT_LIST_HEAD(&rb->event_list);
	spin_lock_init(&rb->event_lock);
}

#ifndef CONFIG_PERF_USE_VMALLOC

/*
 * Back perf_mmap() with regular GFP_KERNEL-0 pages.
 */

struct page *
perf_mmap_to_page(struct ring_buffer *rb, unsigned long pgoff)
{
	if (pgoff > rb->nr_pages)
		return NULL;

	if (pgoff == 0)
		return virt_to_page(rb->user_page);

	return virt_to_page(rb->data_pages[pgoff - 1]);
}

static void *perf_mmap_alloc_page(int cpu)
{
	struct page *page;
	int node;

	node = (cpu == -1) ? cpu : cpu_to_node(cpu);
	page = alloc_pages_node(node, GFP_KERNEL | __GFP_ZERO, 0);
	if (!page)
		return NULL;

	return page_address(page);
}

struct ring_buffer *rb_alloc(int nr_pages, long watermark, int cpu, int flags)
{
	struct ring_buffer *rb;
	unsigned long size;
	int i;

	size = sizeof(struct ring_buffer);
	size += nr_pages * sizeof(void *);

	rb = kzalloc(size, GFP_KERNEL);
	if (!rb)
		goto fail;

	rb->user_page = perf_mmap_alloc_page(cpu);
	if (!rb->user_page)
		goto fail_user_page;

	for (i = 0; i < nr_pages; i++) {
		rb->data_pages[i] = perf_mmap_alloc_page(cpu);
		if (!rb->data_pages[i])
			goto fail_data_pages;
	}

	rb->nr_pages = nr_pages;

	ring_buffer_init(rb, watermark, flags);

	return rb;

fail_data_pages:
	for (i--; i >= 0; i--)
		free_page((unsigned long)rb->data_pages[i]);

	free_page((unsigned long)rb->user_page);

fail_user_page:
	kfree(rb);

fail:
	return NULL;
}

static void perf_mmap_free_page(unsigned long addr)
{
	struct page *page = virt_to_page((void *)addr);

	page->mapping = NULL;
	__free_page(page);
}

void rb_free(struct ring_buffer *rb)
{
	int i;

	perf_mmap_free_page((unsigned long)rb->user_page);
	for (i = 0; i < rb->nr_pages; i++)
		perf_mmap_free_page((unsigned long)rb->data_pages[i]);
	kfree(rb);
}

#else

struct page *
perf_mmap_to_page(struct ring_buffer *rb, unsigned long pgoff)
{
	if (pgoff > (1UL << page_order(rb)))
		return NULL;

	return vmalloc_to_page((void *)rb->user_page + pgoff * PAGE_SIZE);
}

static void perf_mmap_unmark_page(void *addr)
{
	struct page *page = vmalloc_to_page(addr);

	page->mapping = NULL;
}

static void rb_free_work(struct work_struct *work)
{
	struct ring_buffer *rb;
	void *base;
	int i, nr;

	rb = container_of(work, struct ring_buffer, work);
	nr = 1 << page_order(rb);

	base = rb->user_page;
	for (i = 0; i < nr + 1; i++)
		perf_mmap_unmark_page(base + (i * PAGE_SIZE));

	vfree(base);
	kfree(rb);
}

void rb_free(struct ring_buffer *rb)
{
	schedule_work(&rb->work);
}

struct ring_buffer *rb_alloc(int nr_pages, long watermark, int cpu, int flags)
{
	struct ring_buffer *rb;
	unsigned long size;
	void *all_buf;

	size = sizeof(struct ring_buffer);
	size += sizeof(void *);

	rb = kzalloc(size, GFP_KERNEL);
	if (!rb)
		goto fail;

	INIT_WORK(&rb->work, rb_free_work);

	all_buf = vmalloc_user((nr_pages + 1) * PAGE_SIZE);
	if (!all_buf)
		goto fail_all_buf;

	rb->user_page = all_buf;
	rb->data_pages[0] = all_buf + PAGE_SIZE;
	rb->page_order = ilog2(nr_pages);
	rb->nr_pages = 1;

	ring_buffer_init(rb, watermark, flags);

	return rb;

fail_all_buf:
	kfree(rb);

fail:
	return NULL;
}

#endif
