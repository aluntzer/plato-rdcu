/**
 * @file   sysctl.c
 * @ingroup sysctl
 * @author Armin Luntzer (armin.luntzer@univie.ac.at),
 * @author Linus Torvalds et al.
 * @date   January, 2016
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
 * Note: explicit references to linux source files are not always given
 *
 *
 * @defgroup sysctl System Control and Statistics Interface
 *
 * @brief
 *	An interface for examining and dynamically changing of parameters
 *	exported by a driver or other software subsystem to "user space".
 *
 * This system statistics/configuration functionality is tailored/derived
 * from how sysfs and kobjects work in Linux. For obvious reasons, access via a
 * virtual file system is not possible. The file system tree which would usually
 * hold the references to registered objects is not present, hence objects know
 * about their parents AND their children. Sets can be part of another set.
 * Settings and values can be accessed by specifying the path to the object
 * and their name. rees are built from objects and sets. Objects have
 * attributes that can be read and/or set.
 *
 *
 * ## Overview
 *
 * A core necessity of any type of on-board software is the ability to generate
 * housekeeping data to be sent to ground, in order to provide information about
 * the prevailing run-time parameters of both hardware and software.
 * While requirements of update rates and number of variables, especially
 * regarding software, may vary greatly for different mission profiles, there
 * are generally hundreds of these data that are available for selection to
 * form a housekeeping telemetry message. Usually, these are not solely
 * read-only variables, but may also be patched by an appropriate tele-command
 * in order to induce a mode change or adjust parameters to modify the behaviour
 * of the software.
 *
 * These variables may be stored in large, monolithic, globally accessible
 * "data pools". Such simplistic structures may at first glance be the logical
 * choice, suggesting ease of both use and implementation, but are however
 * very susceptible to breakage, particularly in top-down designs, where the
 * data type of the implemented variables is not uniform and interaction with
 * the data structure is only intended to occur via opaque accessor functions.
 *
 * If adjustments are made during development, memory violations may occur
 * during runtime, and those can result in erratic, unpredictable, opaque bugs
 * that are very difficult to track down. Another objection to this type of
 * design is its re-usability, as there may exist multiple points of adaption,
 * especially in circumstances where a great number of internally used
 * variables, which are elemental to a software module or function, are stored
 * in an externally administered data structure.
 *
 * Highly modular, encapsulated software modules with an as minimalistic as
 * possible external interface are very preferable for re-use. Ideally, for
 * example, a SpaceWire driver would only provide an interface to send or
 * receive packets and handle all configuration of the underlying hardware
 * internally. This however poses a problem to a user that would, for
 * example, configure a particular link speed or continuously monitor data
 * transfer rates.
 * For such purposes, an interaction point is needed that exposes certain
 * internal attributes via a generic interface and acts as a conduit between
 * operating system elements and user-space. There are essentially four
 * fundamental requirements for such functionality.
 *
 * First, internal interfaces or variables must not be slower to use than when
 * not exposed. Second, all exposed functionality is defined by the module and
 * exported to the generic interface when initialised. Third, the exposed
 * functionality must not result in unpredictable behaviour, i.e. the software
 * module must be insensitive to sudden changes in states or variables, or care
 * must be taken by the module designer, so that interactions are properly
 * handled. In any case, this must never be a concern for the user. Finally,
 * any  access must be on the user's processing time, not on that of the module.
 *
 * Given that the interaction point has to be completely generic to accommodate
 * any kind of mapping defined by a module without restrictions, it must
 * consequently be very simple. This is most easily achieved by implementing a
 * character-buffer based interface that interacts with a module via functions
 * provided by the latter to the generic interface structure. The necessary
 * parsing or value conversion of text buffers on the user side is obviously
 * slow compared to raw variable access, but given the underlying
 * assumption that this system control interface is to be accessed in the
 * order of no more than a few hundreds or at most several thousands times per
 * second, the overhead is effectively negligible.
 *
 * The concept is very similar to the sysfs and sysctl interfaces found in
 * Linux and BSD operating systems, with the former being file-system driven,
 * while the latter is implemented as a system call. Since a file-system in the
 * classic sense is not typically used for on-board software, this
 * implementation can be seen as a hybrid of the two, which represents nodes in
 * the configuration in the same fashion as a virtual file system tree, while
 * all access is performed via a call interface.
 *
 *
 * ## Mode of Operation
 *
 *
 * __sysobjects__ are objects of type _struct sysobj_. They have a name and
 * contain reference pointers to parent and child objects and the set of
 * sysobjects they belong to, which allows them to be arrangend in hierarchical
 * structures. They have no functionality on their own, instead, they are used
 * to embed functionality as part of another software component.
 *
 * __syssets__ are the basic containers for collections of sysobjects and
 * contain their own sysobject. A sysobject can have any parent, but if the
 * parent of a sysobject is not specified  when it is added to a sysset, the
 * latter automatically becomes the parent of the set, but it does not become
 * the child of the set.
 *
 * __attributes__ define the name and functional I/O of a sysobject.
 *
 *
 *
 * ### Usage example
 * To create a system object for exporting items, a software module must define
 * at least one attribute structure that configures the name and the appropriate
 * _show_ and _store_ methods of that attribute. The following example is
 * modeled on a subsystem that configures the state of an LED between _on_,
 * _off_  and _flash_.
 *
 * First, the states of the LED are defined. This is the configuration variable
 * that sets the operational mode of the "software module". By default, the LED
 * will be in "flash" mode:
 *
 * @code {.c}
 * enum modes {OFF, ON, FLASH};
 * static enum modes mode = FLASH;
 * @endcode
 *
 * Next, _show()_ and _store()_ methods are defined. These allow the retrieval
 * and the definition of the LED mode from user-space:
 *
 * @code {.c}
 * static ssize_t mode_show(struct sysobj *sobj, struct sobj_attribute *sattr,
 *			    char *buf)
 * {
 *	switch(mode) {
 *	case OFF:   return sprintf(buf, "off\n");
 *	case ON:    return sprintf(buf, "on\n");
 *	case FLASH: return sprintf(buf, "flash\n");
 *	default:    return sprintf(buf, "Mode error\n");
 *	}
 * }
 *
 * static ssize_t mode_store(struct sysobj *sobj, struct sobj_attribute *sattr,
 *			     const char *buf, size_t len)
 * {
 *	if (!strncmp(buf, "on", len))
 *		mode = ON;
 *	else if (!strncmp(buf, "off", len))
 *		mode = OFF;
 *	else if (!strncmp(buf, "flash", len))
 *		mode = FLASH;
 *
 *	return len;
 * }
 * @endcode
 *
 *
 * The mode and store functions are then combined into an attribute, that
 * will, in this case, literally be called "mode":
 *
 * @code {.c}
 * static struct sobj_attribute mode_attr = __ATTR(mode, mode_show, mode_store);
 * @endcode
 *
 * The attribute is then added into a set of attributes that is terminated by
 * NULL. A set can contain any number of attributes, in this case it is just
 * one:
 *
 * @code {.c}
 * static struct sobj_attribute *attributes[] = {&mode_attr, NULL};
 * @endcode
 *
 * A system object is created and the attribute set is associated to it:
 *
 * @code {.c}
 * struct sysobj *sobj;
 *
 * sobj = sysobj_create();
 * sobj->sattr = mode_attr;
 * @endcode
 *
 * The system object can now be attached as a node to a set of objects or
 * logical tree, for example to a predefined "/sys/driver" branch under the
 * name "myLEDmodule":
 *
 * @code {.c}
 * sysobj_add(sobj, NULL, driver_set, "myLEDmodule");
 * @endcode
 *
 * The resulting tree can be shown by calling:
 *
 * @code {.c}
 * sysset_show_tree(sys_set);
 * @endcode
 *
 * resulting in the output:
 *
 @verbatim

    |__ sys
	|__ driver
		|__ myLEDmodule { mode }

 @endverbatim
 *
 * The flash mode can now be read by locating the node in the tree and querying
 * "mode":
 *
 * @code {.c}
 * sysobj_show_attr(sysset_find_obj(sys_set,  "/sys/driver/myLEDmodule"), "mode", buf);
 *
 * printf("led mode: %s\n", buf);
 * @endcode
 *
 * resulting in the output:
 *
 @verbatim
    mode: flash
 @endverbatim
 *
 *
 * The flash mode can be changed to value accepted by the _store()_ method:
 *
 * @code {.c}
 * sysobj_store_attr(sysset_find_obj(sys_set, "/sys/driver/myLEDmodule"), "mode", "off", strlen("off"));
 *
 * sysobj_show_attr(sysset_find_obj(sys_set,  "/sys/driver/myLEDmodule"), "mode", buf);
 *
 * printf("led mode: %s\n", buf);
 * @endcode
 *
 * and the output now reads:
 *
 @verbatim
    mode: off
 @endverbatim
 *
 *
 *
 *
 * There are no particular rules on how to use the sysctl tree, but it makes
 * sense to put nodes into appropriate locations.
 * For instance, a SpaceWire driver would register its attributes under a
 * /sys/drivers tree, while an interrupt manager would register under
 * /sys/irq, provided that these sets were already defined.
 *
 * Optionally, a new sub-set to hold the system objects of particular attributes
 * may be created before attaching an object. If the SpaceWire driver was to
 * manage multiple interfaces, it could create a logical sub-set
 * /sys/drivers/spw and group interfaces SpW0, SpW1, etc. under that set.
 *
 * Since there are no formal restrictions on what qualifies to this system
 * configuration tree, application software running on top of the operating
 * system can (and should) make use of it as well. The aforementioned
 * housekeeping data generation makes a good example for an application that
 * both uses the the data provided by the registered software modules to
 * generate housekeeping packets and is itself configured via this interface,
 * e.g. its polling rate and the definition of housekeeping data to collect.
 *
 *
 * ## Error Handling
 *
 * None
 *
 * ## Notes
 *
 * - To accelerate lookup of a node, a hash table should be added, where every
 *   entry is registered. This avoids searching the tree every time.
 * - Ideally, a pointer reference to a particular node would be held when
 *   planning to to repetitive queries on the node. This needs reference
 *   counters so a node is "in use" and cannot be de-registered while a
 *   reference is held. This mandates "grab" and "release" functions to be
 *   called by by a user, which pass a such a reference instead of the raw
 *   pointer.
 * - When reference counters are added, the sysobjects require a release method
 *   so their creating code can be asynchronously notified (see linux kobjects
 *   for similar logic).
 * - At the moment, this is not supposed to be used from mutliple threads or
 *   any other possibly reentrant way. To allow this, modifications need to be
 *   made to at least include reference counts, atomic locking and a strtok_r
 *   function.
 * - Objects and sets are not intended to be freed, this is not implemented due
 *   to the lack of available MMU support
 * - At some point, an extension to binary i/o attributes may be sensible, e.g.
 *   for injection or retrieval of binary calibration data into a module or
 *   subsystem.
 *
 *
 * @example demo_sysctl.c
 *
 */


#include <stdio.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include <list.h>

#include <sysctl.h>


struct sysset *sys_set;
struct sysset *driver_set;




static struct sysset *sysset_get(struct sysset *s);


/**
 * @brief get the name of a sysobject
 * @param sobj a reference to a sysobject
 * @return a reference to the name buffer of the sysobject
 */

const char *sysobj_name(const struct sysobj *sobj)
{
	return sobj->name;
}


/**
 * @brief join a sysobject to its sysset
 * @param sobj a reference to a sysobject
 */

static void sobj_sysset_join(struct sysobj *sobj)
{
	if (!sobj->sysset)
		return;

	sysset_get(sobj->sysset);

	list_add_tail(&sobj->entry, &sobj->sysset->list);
}


/**
 * @brief get the reference to a sysobject
 * @param sobj a reference to a sysobject
 */

static struct sysobj *sysobj_get(struct sysobj *sobj)
{
	return sobj;
}


/**
 * @brief set the name of a sysobject
 * @param sobj a reference to a sysobject
 * @param name the name buffer reference to assign
 */

static void sysobj_set_name(struct sysobj *sobj, const char *name)
{
	sobj->name = name;
}


/**
 * @brief initialise a sysobject's lists head
 * @param sobj a reference to a sysobject
 */

static void sysobj_init_internal(struct sysobj *sobj)
{
	if (!sobj)
		return;

	INIT_LIST_HEAD(&sobj->entry);
}


/**
 * @brief initialise a sysobject
 * @param sobj a reference to a sysobject
 */

void sysobj_init(struct sysobj *sobj)
{
	if (!sobj)
		return;	/* invalid pointer */

	memset(sobj, 0, sizeof(struct sysobj));  /* clear sysobject */
	sysobj_init_internal(sobj);
}


/**
 * @brief create a sysobject
 * @return a reference to the new sysobject or NULL on error
 */

struct sysobj *sysobj_create(void)
{
	struct sysobj *sobj;


	sobj = malloc(sizeof(*sobj));

	if (!sobj)
		return NULL;

	sysobj_init(sobj);

	return sobj;
}


/**
 * @brief add a sysobject and optionally set its sysset as parent
 * @param sobj a reference to the sysobject
 * @return -1 on error, 0 otherwise
 */

static int32_t sysobj_add_internal(struct sysobj *sobj)
{
	struct sysobj *parent;


	if (!sobj)
		return -1;

	parent = sysobj_get(sobj->parent);

	/* join sysset if set, use it as parent if we do not already have one */
	if (sobj->sysset) {
		if (!parent)
			parent = sysobj_get(&sobj->sysset->sobj);
		sobj_sysset_join(sobj);
		sobj->parent = parent;
	}

	return 0;
}


/**
 * @brief add a sysobject to a set and/or a parent
 * @param sobj a reference to the sysobject
 * @param parent a reference to the parent sysobject
 * @param sysset a reference to the parent sysset
 * @param name the name of the sysobj
 * @return -1 on error, 0 otherwise
 */

int32_t sysobj_add(struct sysobj *sobj, struct sysobj *parent,
		   struct sysset *sysset, const char *name)
{
	if (!sobj)
		return -1;

	sobj->sysset = sysset;

	sysobj_set_name(sobj, name);

	sobj->parent = parent;

	sysobj_add_internal(sobj);

	return 0;
}


/**
 * @brief create and add a sysobject to a parent
 * @parm name the name of a sysobj
 * @param parent an optional parent to the sysobject
 * @return a reference to the newly created sysobject or NULL on error
 */

struct sysobj *sysobj_create_and_add(const char *name, struct sysobj *parent)
{
	struct sysobj *sobj;


	sobj = sysobj_create();

	if (!sobj)
		return NULL;

	sysobj_add(sobj, parent, NULL, name);

	return sobj;
}


/**
 * @brief call the "show" attribute function of a sysobject
 * @param sobj a struct sysobj
 * @param name the name of the attribute
 * @param buf a buffer to return parameters
 */

void sysobj_show_attr(struct sysobj *sobj, const char *name, char *buf)
{
	struct sobj_attribute **sattr;


	if (!name)
		return;

	if (!sobj)
		return;

	if (!sobj->sattr)
		return;


	sattr = sobj->sattr;

	while ((*sattr)) {

		if (!strcmp((*sattr)->name, name))
			(*sattr)->show(sobj, (*sattr), buf);

		sattr++;
		continue;

	}
}


/**
 * @brief call the "store" attribute function of a sysobject
 * @param sobj a struct sysobj
 * @param name the name of the attribute
 * @param buf a buffer holding parameters
 * @param len the lenght of the buffer
 */

void sysobj_store_attr(struct sysobj *sobj, const char *name,
		       const char *buf, size_t len)
{
	struct sobj_attribute **sattr;

	if (!name)
		return;

	if (!sobj)
		return;

	if (!sobj->sattr)
		return;


	sattr = sobj->sattr;

	while ((*sattr)) {

		if (!strcmp((*sattr)->name, name))
			(*sattr)->store(sobj, (*sattr), buf, len);

		sattr++;
		continue;

	}
}


/**
 * @brief get the sysset container of a sysobject
 * @param s a struct sysset
 * @return reference to the sysset or NULL
 */

__extension__
static struct sysset *to_sysset(struct sysobj *sobj)
{
	return sobj ? container_of(sobj, struct sysset, sobj) : NULL;
}


/**
 * @brief get the sysset container of a sysset's sysobject
 * @param s a struct sysset
 * @return reference to the sysset or NULL
 */

static struct sysset *sysset_get(struct sysset *s)
{
	return s ? to_sysset(sysobj_get(&s->sobj)) : NULL;
}


/**
 * @brief initialise a sysset
 * @param s a struct sysset
 */

static void sysset_init(struct sysset *s)
{
	sysobj_init_internal(&s->sobj);
	INIT_LIST_HEAD(&s->list);
}


/**
 * @brief register a sysset
 * @param s a struct sysset
 * @return -1 on error, 0 otherwise
 */

static int32_t sysset_register(struct sysset *s)
{
	if (!s)
		return -1;

	sysset_init(s);

	return sysobj_add_internal(&s->sobj);
}


/**
 * @brief create a sysset
 * @param name a string holding the name of the set
 * @param parent_sobj a struct sysobj or NULL if no sysobj parent
 * @param parent_sysset a struct sysysset or NULL if no sysysset parent
 * @return a reference to the new sysset
 */

struct sysset *sysset_create(const char *name,
			     struct sysobj *parent_sobj,
			     struct sysset *parent_sysset)
{
	struct sysset *sysset;

	sysset = calloc(1, sizeof(*sysset));

	if (!sysset)
		return NULL;

	sysobj_set_name(&sysset->sobj, name);

	sysset->sobj.parent = parent_sobj;
	sysset->sobj.child  = &sysset->sobj;

	sysset->sobj.sysset = parent_sysset;

	return sysset;
}


/**
 * @brief create and add a sysset
 * @param name a string holding the name of the set
 * @param parent_sobj a struct sysobj or NULL if no sysobj parent
 * @param parent_sysset a struct sysysset or NULL if no sysysset parent
 * @return a reference to the new sysset
 */

struct sysset *sysset_create_and_add(const char *name,
				     struct sysobj *parent_sobj,
				     struct sysset *parent_sysset)
{
	struct sysset *sysset;


	sysset = sysset_create(name, parent_sobj, parent_sysset);

	if (!sysset)
		return NULL;

	sysset_register(sysset);

	return sysset;
}


/**
 * @brief find the reference to an object in a sysset by its path
 * @param sysset a struct sysset
 * @param path a string describing a path
 * @return a reference to the sysobj found
 *
 * @note this function is guaranteed to return, unless the machine's memory
 *	 is either effectively a loop or infinite, but in this one instance.
 *	 polyspace static analyzer appears fail here, the unit test clearly
 *	 demonstrates that all sections of this function are reachable
 *
 * @note this function was modified to use a common exit point
 */

__extension__
struct sysobj *sysset_find_obj(struct sysset *sysset, const char *path)
{
	char str[256];
	char *token;

	struct sysobj *s;
	struct sysobj *ret = NULL;



	memcpy(str, path, strlen(path) + 1);

	token = strtok(str, "/");

	/* root node */
	if (strcmp(sysobj_name(&sysset->sobj), token))
		goto exit;

	while (1) {

		token = strtok(NULL, "/");

		if (!token)
			goto exit;

		list_for_each_entry(s, &sysset->list, entry) {

			if (!sysobj_name(s))
				goto exit;

			if (strcmp(sysobj_name(s), token))
				continue;

			if (!s->child) {
				ret = s;
				goto exit;
			}

			sysset = container_of(s->child, struct sysset, sobj);

			break;
		}
	}

exit:
	return ret;
}


#if (__sparc__)
/**
 * @brief initalises sysctl
 * @note will not be covered by unit test
 */
int32_t sysctl_init(void)
{
	if (sys_set)
		return -1;

	sys_set = sysset_create_and_add("sys", NULL, NULL);

	if (!sys_set)
		return -1;

	driver_set = sysset_create_and_add("driver", NULL, sys_set);

	if (!driver_set)
		return -1;

	return 0;
}
#endif
