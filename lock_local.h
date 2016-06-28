/**
 * \file
 * \brief     Davrods locking support using local locks.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2016, Utrecht University
 *
 * This file is part of Davrods.
 *
 * Davrods is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Davrods is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Davrods.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _DAVRODS_LOCK_H_
#define _DAVRODS_LOCK_H_

#include "common.h"

extern const dav_hooks_locks davrods_hooks_locallock;

typedef struct davrods_locklocal_lock_list_t {
    const char *entry;
    struct davrods_locklocal_lock_list_t *next;
} davrods_locklocal_lock_list_t;

/**
 * \brief Get a list of locked entries in the given collection.
 *
 * \param[in]  lockdb
 * \param[in]  col    a collection resource
 * \param[out] names
 *
 * \return a dav error, if bad stuff happens
 */
dav_error *davrods_locklocal_get_locked_entries(
    dav_lockdb *lockdb,
    const dav_resource *col,
    davrods_locklocal_lock_list_t **names
);

#endif /* _DAVRODS_LOCK_H_ */
