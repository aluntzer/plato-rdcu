/**
 * @file    irq_dispatch.c
 * @ingroup irq_dispatch
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 * @date    December, 2013
 * @brief   Central IRQ dispatcher
 *
 * @copyright
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @note eventually replace catch_interrupt() libgloss/newlib functionality
 *       with local/custom code and rework the globals
 * @note irq configuration should only be done through a syscall,
 *       so traps are disabled
 *
 * @defgroup irq_dispatch IRQ dispatcher
 * @brief a central IRQ handler that dispatches interrupts to registered
 *        callback functions
 *
 * Implements a central interrupt handler that supports registration of a
 * predefined arbitrary number of callback function per interrupt with immediate
 * or deferred execution priorities. Callbacks are tracked in linked lists that
 * should always be allocated in a contiguous block of memory for proper cache
 * hit rate.
 *
 * @image html irq_dispatch/irq_dispatch.png "IRQ dispatcher"
 *
 * On LEONs with round-robin extended interrupt
 * lines, such as in the MPPB, the handler can be modified to execute all active
 * secondary interrupts without exiting IRQ mode for significantly reduced
 * call overhead.
 *
 * @example demo_irq_dispatch.c
 */


#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "irq_dispatch.h"
#include "../lib/common/list.h"
#include "irq.h"
#include "spinlock.h"
#include "leon_reg.h"
#include "../lib/common/compiler.h"
#include "errors.h"
#include "asm/leon.h"
#include "io.h"
#include "sysctl.h"

#if !(__sparc__)
/* unit testing dummy */
int catch_interrupt(__attribute__((unused)) int func,
		     __attribute__((unused)) int irq)
{
	return 0;
}
#else
#include "irq.h"
#endif



struct irl_vector_elem {
	int32_t (*callback)(void *userdata);
	enum prty priority;
	void *userdata;
	struct list_head callback_node;
};

#define IRL_POOL_SIZE	128
#define IRL_QUEUE_SIZE	128
#define IRL_SIZE	128

struct list_head	irl_pool_head;
struct list_head	irl_queue_head;
struct list_head	irq_queue_pool_head;

struct list_head	irq1_vector[IRL_SIZE];
struct list_head	irq2_vector[IRL_SIZE];

struct irl_vector_elem	irl_pool[IRL_POOL_SIZE];
struct irl_vector_elem	irl_queue_pool[IRL_QUEUE_SIZE];



/* temporarily from c-irq.c */
struct leon3_irqctrl_registermap *leon3_irqctrl_regs;

#define ICLEAR 0x20c
#define IMASK  0x240
#define IFORCE 0x208

#if (__sparc__)
int32_t *lreg = (int32_t *) 0x80000000;

void enable_irq(int32_t irq)
{
	lreg[ICLEAR/4] = (1 << irq);    /* clear any pending irq*/
	lreg[IMASK/4] |= (1 << irq);    /* unmaks irq		*/
	leon3_irqctrl_regs->irq_mpmask[leon3_cpuid()] |= 1 << irq;
}

void enable_irq2(int32_t irq)
{
	lreg[ICLEAR/4] = (1 << irq);    /* clear any pending irq*/
	lreg[IMASK/4] |= (1 << irq);    /* unmaks irq		*/
	leon3_irqctrl_regs->irq_mpmask[leon3_cpuid()] |= (1 << irq) << 16;
}

void disable_irq(int32_t irq)
{
	lreg[IMASK/4] &= ~(1 << irq);
}

void disable_irq2(int32_t irq)
{
	lreg[IMASK/4] &= ~((1 << irq) << 16);
}

void force_irq(int32_t irq)
{
	lreg[IFORCE/4] = (1 << irq);
}
#else
void enable_irq(int32_t __attribute__((unused)) irq)
{
}

void enable_irq2(int32_t __attribute__((unused)) irq)
{
}

void disable_irq(int32_t __attribute__((unused)) irq)
{
}

void disable_irq2(int32_t __attribute__((unused)) irq)
{
}
#endif
/* end temp */


static struct {
	uint32_t irl1;
	uint32_t irl2;
	uint32_t irl1_irq[15];
	uint32_t irl2_irq[15];
} irqstat;

#if (__sparc__)
#define UINT32_T_FORMAT		"%lu"
#else
#define UINT32_T_FORMAT		"%u"
#endif

static ssize_t irl1_show(__attribute__((unused)) struct sysobj *sobj,
			 __attribute__((unused)) struct sobj_attribute *sattr,
			 char *buf)
{
	uint32_t irq;


	if (!strcmp("irl1", sattr->name))
		return sprintf(buf, UINT32_T_FORMAT, irqstat.irl1);

	irq = atoi(sattr->name);

	if (irq > 15)
		return 0;

	return sprintf(buf, UINT32_T_FORMAT, irqstat.irl1_irq[irq - 1]);
}

static ssize_t irl1_store(__attribute__((unused)) struct sysobj *sobj,
			  __attribute__((unused)) struct sobj_attribute *sattr,
			  __attribute__((unused)) const char *buf,
			  __attribute__((unused)) size_t len)
{
	uint32_t irq;


	if (!strcmp("irl1", sattr->name)) {
		irqstat.irl1 = 0;
		return 0;
	}

	irq = atoi(sattr->name);

	if (irq > 15)
		return 0;

	irqstat.irl1_irq[irq - 1] = 0;

	return 0;
}

static ssize_t irl2_show(__attribute__((unused)) struct sysobj *sobj,
			 __attribute__((unused)) struct sobj_attribute *sattr,
			 char *buf)
{
	uint32_t irq;


	if (!strcmp("irl2", sattr->name))
		return sprintf(buf, UINT32_T_FORMAT, irqstat.irl2);

	irq = atoi(sattr->name);

	if (irq > 15)
		return 0;

	return sprintf(buf, UINT32_T_FORMAT, irqstat.irl2_irq[irq - 1]);
}

static ssize_t irl2_store(__attribute__((unused)) struct sysobj *sobj,
			  __attribute__((unused)) struct sobj_attribute *sattr,
			  __attribute__((unused)) const char *buf,
			  __attribute__((unused)) size_t len)
{
	uint32_t irq;


	if (!strcmp("irl2", sattr->name)) {
		irqstat.irl2 = 0;
		return 0;
	}

	irq = atoi(sattr->name);

	if (irq > 15)
		return 0;

	irqstat.irl2_irq[irq - 1] = 0;

	return 0;
}

__extension__
static struct sobj_attribute irl1_attr[] = {
	__ATTR(irl1,  irl1_show, irl1_store),
	__ATTR(1,  irl1_show, irl1_store), __ATTR(2,  irl1_show, irl1_store),
	__ATTR(3,  irl1_show, irl1_store), __ATTR(4,  irl1_show, irl1_store),
	__ATTR(5,  irl1_show, irl1_store), __ATTR(6,  irl1_show, irl1_store),
	__ATTR(7,  irl1_show, irl1_store), __ATTR(8,  irl1_show, irl1_store),
	__ATTR(9,  irl1_show, irl1_store), __ATTR(10, irl1_show, irl1_store),
	__ATTR(11, irl1_show, irl1_store), __ATTR(12, irl1_show, irl1_store),
	__ATTR(13, irl1_show, irl1_store), __ATTR(14, irl1_show, irl1_store),
	__ATTR(15, irl1_show, irl1_store)};

static struct sobj_attribute *irl1_attributes[] = {
	&irl1_attr[0],  &irl1_attr[1],  &irl1_attr[2],  &irl1_attr[3],
	&irl1_attr[4],  &irl1_attr[5],  &irl1_attr[6],  &irl1_attr[7],
	&irl1_attr[8],  &irl1_attr[9],  &irl1_attr[10], &irl1_attr[11],
	&irl1_attr[12], &irl1_attr[13], &irl1_attr[14], &irl1_attr[14],
	NULL};

__extension__
static struct sobj_attribute irl2_attr[] = {
	__ATTR(irl2,  irl2_show, irl2_store),
	__ATTR(1,  irl2_show, irl2_store), __ATTR(2,  irl2_show, irl2_store),
	__ATTR(3,  irl2_show, irl2_store), __ATTR(4,  irl2_show, irl2_store),
	__ATTR(5,  irl2_show, irl2_store), __ATTR(6,  irl2_show, irl2_store),
	__ATTR(7,  irl2_show, irl2_store), __ATTR(8,  irl2_show, irl2_store),
	__ATTR(9,  irl2_show, irl2_store), __ATTR(10, irl2_show, irl2_store),
	__ATTR(11, irl2_show, irl2_store), __ATTR(12, irl2_show, irl2_store),
	__ATTR(13, irl2_show, irl2_store), __ATTR(14, irl2_show, irl2_store),
	__ATTR(15, irl2_show, irl2_store)};

__extension__
static struct sobj_attribute *irl2_attributes[] = {
	&irl2_attr[0],  &irl2_attr[1],  &irl2_attr[2],  &irl2_attr[3],
	&irl2_attr[4],  &irl2_attr[5],  &irl2_attr[6],  &irl2_attr[7],
	&irl2_attr[8],  &irl2_attr[9],  &irl2_attr[10], &irl2_attr[11],
	&irl2_attr[12], &irl2_attr[13], &irl2_attr[14], &irl2_attr[15],
	NULL};

/**
 * @brief	queue a callback for delayed exectuion
 *
 * @param[in]	p_elem	a pointer to a struct IRQVectorElem
 *
 * @retval	-1 : error
 * @retval	 0 : success
 */

static int32_t irq_queue(struct irl_vector_elem *p_elem)
{
	uint32_t psr_flags;
	struct irl_vector_elem *p_queue;

	if (likely(list_filled(&irq_queue_pool_head))) {

		psr_flags = spin_lock_save_irq();

		p_queue = list_entry((&irq_queue_pool_head)->next,
				      struct irl_vector_elem, callback_node);

		p_queue->callback = p_elem->callback;
		p_queue->priority = p_elem->priority;
		p_queue->userdata = p_elem->userdata;

		list_move_tail(&p_queue->callback_node, &irl_queue_head);

		spin_lock_restore_irq(psr_flags);

		return 0;

	} else {
		errno = E_IRQ_QUEUE_BUSY;
		return -1;
	}
}


/**
 * @brief	the central interrupt handling routine
 *
 * @param[in]	irq an interrupt number
 *
 * @retval	0 : always
 *
 * @note callback return codes ignored for now
 */

static int32_t irq_dispatch(int32_t irq)
{
	uint32_t irq2;

	struct irl_vector_elem *p_elem;
	struct irl_vector_elem *p_tmp;


	irqstat.irl1++;
	irqstat.irl1_irq[irq - 1]++;

	if (irq == IRL1_EXTENDED_INT) {

		irq2 = leon3_irqctrl_regs->extended_irq_id[leon3_cpuid()];

		irqstat.irl2++;
		irqstat.irl2_irq[irq2 - 1]++;

		list_for_each_entry_safe(p_elem, p_tmp,
					 &irq2_vector[irq2], callback_node) {

			if (likely(p_elem->priority == PRIORITY_NOW)) {
				p_elem->callback(p_elem->userdata);
			} else {
				/* execute immediately if queueing fails */
				if (irq_queue(p_elem) < 0)
					p_elem->callback(p_elem->userdata);
			}
		}

	} else { /* regular (primary) interrupts */

		list_for_each_entry(p_elem, &irq1_vector[irq], callback_node) {
			if (likely(p_elem->priority == PRIORITY_NOW)) {
				p_elem->callback(p_elem->userdata);
			} else {
				/* execute immediately if queueing fails */
				if (irq_queue(p_elem) < 0)
					p_elem->callback(p_elem->userdata);
			}
		}
	}

	return 0;
}



/**
 * @brief	register a callback function to the primary interrupt line
 *
 *
 * @param[in]	irq		an interrupt number
 * @param[in]	priority	a callback priority
 * @param[in]	callback	a callback function
 * @param[in]	userdata	a pointer to arbitrary user data; passed to
 *				 callback function
 *
 * @retval	-1 : error
 * @retval	 0 : success
 *
 * @bug		catch_interrupt() is called without checking whether the IRL
 *		was already mapped
 */

int32_t irl1_register_callback(int32_t irq,
			       enum prty priority,
			       int32_t (*callback)(void *userdata),
			       void *userdata)
{
	uint32_t psr_flags;

	struct irl_vector_elem *p_elem;


	if (irq >= IRL_SIZE) {
		errno = E_IRQ_EXCEEDS_IRL_SIZE;
		return -1;
	}

	if (list_empty(&irl_pool_head)) {
		errno = E_IRQ_POOL_EMPTY;
		return -1;
	}

	if (callback == NULL) {
		errno = EINVAL;
		return -1;
	}

	psr_flags = spin_lock_save_irq();

	p_elem = list_entry((&irl_pool_head)->next, struct irl_vector_elem,
			    callback_node);

	p_elem->callback = callback;
	p_elem->priority = priority;
	p_elem->userdata = userdata;

	list_move_tail(&p_elem->callback_node, &irq1_vector[irq]);

	spin_lock_restore_irq(psr_flags);

	enable_irq(irq);

	return catch_interrupt(((int) irq_dispatch), irq);
}


/**
 * @brief	de-register a callback function on the primary interrupt line
 *
 * @param[in]	IRQ		an interrupt number
 * @param[in]	callback	a callback function
 * @param[in]	userdata	a pointer to arbitrary user data
 *
 * @retval	-1 : error
 * @retval	 0 : success
 *
 * @note	in case of duplicate callbacks, only the first encountered will
 *		be removed
 */

int32_t irl1_deregister_callback(int32_t irq,
				 int32_t (*callback)(void *userdata),
				 void *userdata)
{
	uint32_t psr_flags;
	struct irl_vector_elem *p_elem;
	struct irl_vector_elem *p_tmp;

	if (irq >= IRL_SIZE) {
		errno = E_IRQ_EXCEEDS_IRL_SIZE;
		return -1;
	}

	list_for_each_entry_safe(p_elem, p_tmp,
				 &irq1_vector[irq], callback_node) {

		if (p_elem->callback == callback
		    && p_elem->userdata == userdata) {

			p_elem->callback = NULL;
			p_elem->userdata = NULL;
			p_elem->priority = -1;

			psr_flags = spin_lock_save_irq();
			list_move_tail(&p_elem->callback_node, &irl_pool_head);
			spin_lock_restore_irq(psr_flags);

			if (irq1_vector[irq].next == &irq1_vector[irq])
				disable_irq(irq);

			return 0;
		}
	}
	errno = E_IRQ_DEREGISTER;
	return -1;
}


/**
 * @brief	register a callback function to the secondary interrupt line
 *
 * @param[in]	IRQ		an interrupt number
 * @param[in]	priority	a callback priority
 * @param[in]	callback	a callback function
 * @param[in]	userdata	a pointer to arbitrary user data
 *
 * @retval	-1 : error
 * @retval	 0 : success
 *
 */

int32_t irl2_register_callback(int32_t irq,
			       enum prty priority,
			       int32_t (*callback)(void *userdata),
			       void *userdata)
{
	uint32_t psr_flags;
	struct irl_vector_elem *p_elem;

	if (irq >= IRL_SIZE) {
		errno = E_IRQ_EXCEEDS_IRL_SIZE;
		return -1;
	}

	if (list_empty(&irl_pool_head)) {
		errno = E_IRQ_POOL_EMPTY;
		return -1;
	}

	if (callback == NULL) {
		errno = EINVAL;
		return -1;
	}

	psr_flags = spin_lock_save_irq();
	p_elem = list_entry((&irl_pool_head)->next, struct irl_vector_elem,
		    callback_node);

	p_elem->callback = callback;
	p_elem->priority = priority;
	p_elem->userdata = userdata;

	list_move_tail(&p_elem->callback_node, &irq2_vector[irq]);

	spin_lock_restore_irq(psr_flags);

	enable_irq2(irq);

	return 0;
}

/**
 * @brief	de-register a callback function on the secondary interrupt line
 *
 * @param[in]	IRQ		an interrupt number
 * @param[in]	callback	a callback function
 * @param[in]	userdata	a pointer to arbitrary user data
 *
 * @retval	-1 : error
 * @retval	 0 : success
 */

int32_t irl2_deregister_callback(int32_t irq,
				 int32_t (*callback)(void *userdata),
				 void *userdata)
{
	uint32_t psr_flags;
	struct irl_vector_elem *p_elem;
	struct irl_vector_elem *p_tmp;

	if (irq >= IRL_SIZE) {
		errno = E_IRQ_EXCEEDS_IRL_SIZE;
		return -1;
	}

	list_for_each_entry_safe(p_elem, p_tmp,
				 &irq2_vector[irq], callback_node) {

		if (p_elem->callback == callback
		    && p_elem->userdata == userdata) {

			p_elem->callback = NULL;
			p_elem->userdata = NULL;
			p_elem->priority = -1;

			psr_flags = spin_lock_save_irq();
			list_move_tail(&p_elem->callback_node, &irl_pool_head);
			spin_lock_restore_irq(psr_flags);

			if (irq2_vector[irq].next == &irq2_vector[irq])
				disable_irq2(irq);

			return 0;
		}
	}

	errno = E_IRQ_DEREGISTER;
	return -1;
}

/**
 *
 * @brief	call this function in normal mode to handle non-priority
 *		interrupt requests
 *
 */

void irq_queue_execute(void)
{
	struct irl_vector_elem *p_elem = NULL;
	struct irl_vector_elem *p_tmp;


	if (list_empty(&irl_queue_head))
		return;

	list_for_each_entry_safe(p_elem, p_tmp,
				 &irl_queue_head, callback_node) {

		list_del(&p_elem->callback_node);

		if (likely(p_elem->callback != NULL)) {

			if (p_elem->callback(p_elem->userdata))
				irq_queue(p_elem);
			else
				list_add_tail(&p_elem->callback_node,
					      &irq_queue_pool_head);

		} else {
			list_add_tail(&p_elem->callback_node,
				      &irq_queue_pool_head);
		}
	}
}


void irq_set_level(uint32_t irq_mask, uint32_t level)
{
	uint32_t flags;

	flags = ioread32be(&leon3_irqctrl_regs->irq_level);

	if (!level)
		flags &= ~irq_mask;
	else
		flags |= irq_mask;


	iowrite32be(flags, &leon3_irqctrl_regs->irq_level);
}



/**
 * @brief	enable the interrupt handling service
 *
 * @retval	-1 : error
 * @retval	 0 : success
 */

int32_t irq_dispatch_enable(void)
{
	size_t i;

	struct sysset *sset;
	struct sysobj *sobj;

#if (__sparc__)
	/**
	 * Basic Moron Protector (BMP)™
	 */
	static enum  {DISABLED, ENABLED} dispatcher;

	if (dispatcher)
		return -1;

	dispatcher = ENABLED;
#endif

	INIT_LIST_HEAD(&irl_pool_head);
	INIT_LIST_HEAD(&irl_queue_head);
	INIT_LIST_HEAD(&irq_queue_pool_head);

	for (i = 0; i < IRL_SIZE; i++) {
		INIT_LIST_HEAD(&irq1_vector[i]);
		INIT_LIST_HEAD(&irq2_vector[i]);
	}

	for (i = 0; i < IRL_POOL_SIZE; i++) {
		irl_pool[i].callback = NULL;
		irl_pool[i].userdata = NULL;
		list_add_tail(&irl_pool[i].callback_node, &irl_pool_head);
	}

	for (i = 0; i < IRL_QUEUE_SIZE; i++) {
		irl_queue_pool[i].callback = NULL;
		irl_queue_pool[i].userdata = NULL;
		list_add_tail(&irl_queue_pool[i].callback_node,
			      &irq_queue_pool_head);
	}


	sset = sysset_create_and_add("irq", NULL, sys_set);

	sobj = sysobj_create();

	if (!sobj)
		return -1;

	sobj->sattr = irl1_attributes;
	sysobj_add(sobj, NULL, sset, "primary");

	sobj = sysobj_create();

	if (!sobj)
		return -1;

	sobj->sattr = irl2_attributes;
	sysobj_add(sobj, NULL, sset, "secondary");


	leon3_irqctrl_regs = (struct leon3_irqctrl_registermap *)
						LEON3_BASE_ADDRESS_IRQMP;

	enable_irq(IRL1_EXTENDED_INT);


	/* workaround for v0.8:
	 * enable timer 0 and 1 irqs so their interrupts are counted
	 * by irq_dispatch as well
	 */
	catch_interrupt(((int) irq_dispatch), GR712_IRL1_GPTIMER_0);
	catch_interrupt(((int) irq_dispatch), GR712_IRL1_GPTIMER_1);

	/* retval check can be done outside */
	return catch_interrupt(((int) irq_dispatch), GR712_IRL1_IRQMP);
}
