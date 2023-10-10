/**
 * @file    spinlock.h
 * @author  Armin Luntzer (armin.luntzer@univie.ac.at)
 * @date    December, 2013
 * @brief   MPPB Spin Locking Mechanism
 *
 * @copyright Armin Luntzer (armin.luntzer@univie.ac.at)
 * @copyright David S. Miller (davem@caip.rutgers.edu), 1997  (some parts)
 *
 * @copyright
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

#include "../lib/common/compiler.h"

struct spinlock {
	volatile uint8_t	lock;
	volatile uint32_t	lock_recursion;
};


/* the sparc PIL field	*/
#define PSR_PIL 0x00000f00


#if (__sparc__)



/**
 * @brief save and disable the processor interrupt level state
 * @return PSR
 * @warning make sure to call a save/restore pair from within the same stack frame
 */

static uint32_t spin_lock_save_irq(void)
{
	uint32_t psr;
	uint32_t tmp;
	/**
	 * @note RDPSR and WRPSR are only available privileged mode and will trap otherwise
	 * @note The three NOPs after WRPSR could in principle be left out,
	 * @note but we keep them nonetheless, as the SPARC (V7.0) instruction manual states:
	 * @note 3. If any of the three instructions after a WRPSR instruction reads
	 * @note the modified PSR, the value read is unpredictable.
	 */
	__asm__ __volatile__(
			"rd     %%psr, %0        \n\t"
			"or        %0, %2, %1    \n\t"
			"wr        %1,  0, %%psr \n\t"
			"nop                     \n\t"
			"nop                     \n\t"
			"nop                     \n\t"
			: "=&r" (psr), "=r" (tmp)
			: "i"   (PSR_PIL)
			: "memory");
	return psr;
}

/**
 * @brief Restore the processor interrupt level state
 */

static void spin_lock_restore_irq(uint32_t psr)
{
	uint32_t tmp;

	__asm__ __volatile__(
			"rd     %%psr, %0        \n\t"
			"and       %2, %1, %2    \n\t"
			"andn      %0, %1, %0    \n\t"
			"wr        %0, %2, %%psr \n\t"
			"nop                     \n\t"
			"nop                     \n\t"
			"nop                     \n\t"
			: "=&r" (tmp)
			: "i"   (PSR_PIL), "r" (psr)
			: "memory");
}


/**
 * @brief	MPPB LEON-side spin lock
 * @param 	p_lock a struct spinlock
 *
 * @warning	will silently fail AND deadlock everytime YOU are stupid
 * @note	it is, however, save to use with interrupts (sort of)
 */
__attribute__((unused))
static void spin_lock(struct spinlock *p_lock)
{
	uint32_t psr_flags;

	if (unlikely(p_lock->lock_recursion))
		return;

	psr_flags = spin_lock_save_irq();

	p_lock->lock_recursion = 1;

	__asm__ __volatile__(
			"1:                      \n\t"
			"ldstub [%0], %%g2       \n\t"
			"andcc  %%g2, %%g2, %%g2 \n\t"
			"bnz,a    1b             \n\t"
			" nop		         \n\n"
			:
			: "r" (&p_lock->lock)
			: "g2", "memory", "cc");

	p_lock->lock_recursion = 0;

	spin_lock_restore_irq(psr_flags);
}

/**
 * @brief	MPPB LEON-side spin lock which does not care about interrupts
 * @param 	p_lock a struct spinlock
 *
 * @warning	will silently fail AND deadlock everytime YOU are stupid
 */

__attribute__((unused))
static void spin_lock_raw(struct spinlock *p_lock)
{
	if (unlikely(p_lock->lock_recursion))
		return;

	p_lock->lock_recursion = 1;

	__asm__ __volatile__(
			"1:                      \n\t"
			"ldstub [%0], %%g2       \n\t"
			"andcc  %%g2, %%g2, %%g2 \n\t"
			"bnz,a    1b             \n\t"
			" nop		         \n\n"
			:
			: "r" (&p_lock->lock)
			: "g2", "memory", "cc");

	p_lock->lock_recursion = 0;
}

/**
 *@brief 	lock check
 *@returns	success or failure
 */

static int spin_is_locked(struct spinlock *p_lock)
{
	return (p_lock->lock != 0);
}

/**
 *@brief 	spins until lock opens
 */

__attribute__((unused))
static void spin_unlock_wait(struct spinlock *p_lock)
{
	__asm__ __volatile__ ("" : : : "memory");	/* optimization barrier, just in case */
	while(spin_is_locked(p_lock));
}

/**
 * @brief	MPPB LEON-side spin lock
 * @param 	p_lock a struct spinlock
 * @return	success or failure
 *
 * @note
 */

__attribute__((unused))
static int spin_try_lock(struct spinlock *p_lock)
{
	uint32_t retval;

	__asm__ __volatile__("ldstub [%1], %0" : "=r" (retval) : "r"  (&p_lock->lock) : "memory");

	return (retval == 0);
}

/**
 * @brief	MPPB LEON-side spin-unlock
 * @param 	p_lock a struct spinlock
 */

__attribute__((unused))
static void spin_unlock(struct spinlock *p_lock)
{
	__asm__ __volatile__("swap [%0], %%g0       \n\t" : : "r" (&p_lock->lock) : "memory");
}


#else /*!(__sparc__)*/

__attribute__((unused))
static uint32_t spin_lock_save_irq(void)
{
	return 0;
}

__attribute__((unused))
static void spin_lock_restore_irq(__attribute__((unused)) uint32_t psr)
{
}

#if (__unused__)
__attribute__((unused))
static void spin_lock(__attribute__((unused)) struct spinlock *p_lock)
{
}

__attribute__((unused))
static void spin_lock_raw(__attribute__((unused)) struct spinlock *p_lock)
{
}

__attribute__((unused))
static int spin_is_locked(__attribute__((unused)) struct spinlock *p_lock)
{
	return 0;
}

__attribute__((unused))
static void spin_unlock_wait(__attribute__((unused)) struct spinlock *p_lock)
{
}

__attribute__((unused))
static int spin_try_lock(__attribute__((unused)) struct spinlock *p_lock)
{
	return 0;
}

__attribute__((unused))
static void spin_unlock(__attribute__((unused)) struct spinlock *p_lock)
{
}
#endif /* (__unused__) */

#endif /*!(__sparc__)*/


#endif
