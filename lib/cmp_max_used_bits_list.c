/**
 * @file   cmp_max_used_bits_list.c
 * @author Dominik Loidolt (dominik.loidolt@univie.ac.at)
 * @date   2023
 *
 * @copyright GPLv2
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * @brief Implement a list that can hold an arbitrary number of different
 *	cmp_max_used_bits structs
 *
 * @warning not intended for use with the flight software
 */


#include <stdlib.h>
#include <assert.h>

#include <list.h>
#include <cmp_max_used_bits.h>


struct list_item {
	struct list_head list;
	struct cmp_max_used_bits data;
};

LIST_HEAD(max_used_bits_list);


/**
 * @brief get an item from the max_used_bits list
 *
 * @param version  version identifier of the max_used_bits list item
 *
 * @returns a pointer to the max_used_bits structure with the corresponding version
 *	on success; NULL if no list item has the version number
 */

const struct cmp_max_used_bits *cmp_max_used_bits_list_get(uint8_t version)
{
	struct list_item *list_ptr = NULL;

	switch (version) {
	case 0:
		return &MAX_USED_BITS_SAFE;
	case 1:
		return &MAX_USED_BITS_V1;

	}

	list_for_each_entry(list_ptr, &max_used_bits_list, list) {
		if (list_ptr->data.version == version)
			return &list_ptr->data;
	}
	return NULL;
}


/**
 * @brief add a max_used_bits item to the list
 *
 * @param item  pointer to the cmp_max_used_bits struct to add to the list
 *
 * @note if there is an item in the list with the same version number, it will
 *	be overwritten
 * @returns 0 on success; 1 if an existing entry is overwritten; -1 on error
 */

int cmp_max_used_bits_list_add(struct cmp_max_used_bits const *item)
{
	struct list_item *item_ptr = NULL;
	struct cmp_max_used_bits *p;

	if (!item)
		return -1;
	if (item->version <= 16)
		return -1;

	p = (struct cmp_max_used_bits *)cmp_max_used_bits_list_get(item->version);
	if (p) {
		*p = *item; /* replace existing list entry */
		return 1;
	}

	item_ptr = (struct list_item *)malloc(sizeof(struct list_item));
	if (!item_ptr)
		return -1;

	item_ptr->data = *item;
	list_add(&item_ptr->list, &max_used_bits_list);

	return 0;
}


/**
 * @brief delete a max_used_bits item from the list
 *
 * @param version  version identifier of the max_used_bits list entry to be deleted
 *
 * @note if no max_used_bits list item has the version identifier, nothing happens
 */

void cmp_max_used_bits_list_delet(uint8_t version)
{
	struct list_item *list_ptr = NULL;
	struct list_item *tmp = NULL;

	list_for_each_entry_safe(list_ptr, tmp, &max_used_bits_list, list) {
		if (list_ptr->data.version == version) {
			list_del(&list_ptr->list);
			free(list_ptr);
			list_ptr = NULL;
		}
	}
}


/**
 * @brief delete all max_used_bits item from the list
 */

void cmp_max_used_bits_list_empty(void)
{
	struct list_item *list_ptr = NULL;
	struct list_item *tmp = NULL;

	list_for_each_entry_safe(list_ptr, tmp, &max_used_bits_list, list) {
		list_del(&list_ptr->list);
		free(list_ptr);
		list_ptr = NULL;
	}
}
