/**
 * \file
 * \brief     Davrods HTML listing support.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2016-2020, Utrecht University
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
#ifndef _DAVRODS_LISTING_H
#define _DAVRODS_LISTING_H

#include "common.h"

/// Send a HTML directory listing as a response to a web browser request.
/// 'resource' must be a collection.
dav_error *davrods_deliver_directory_listing(
    const dav_resource *resource,
    ap_filter_t *output
);

#endif /* _DAVRODS_LISTING_H */
