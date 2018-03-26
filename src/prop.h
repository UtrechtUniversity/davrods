/**
 * \file
 * \brief     Davrods DAV property support.
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
#ifndef _DAVRODS_PROP_H_
#define _DAVRODS_PROP_H_

#include "common.h"

extern const char *const davrods_namespace_uris[];
extern const dav_hooks_liveprop davrods_hooks_liveprop;
extern const dav_liveprop_spec  davrods_props[];
extern const dav_liveprop_group davrods_liveprop_group;
extern const size_t DAVRODS_PROP_COUNT;

#endif /* _DAVRODS_PROP_H_ */
