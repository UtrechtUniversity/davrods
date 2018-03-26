/**
 * \file
 * \brief     Davrods GET+Range request support.
 * \author    Chris Smeele
 * \copyright Copyright (c) 2018, Utrecht University
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
#ifndef _DAVRODS_BYTERANGE_H
#define _DAVRODS_BYTERANGE_H

#include "common.h"

#include <irods/rodsClient.h>
#include <irods/rods.h>

dav_error *davrods_byterange_deliver_file(const dav_resource *resource,
                                          openedDataObjInp_t *data_obj,
                                          ap_filter_t        *output,
                                          apr_bucket_brigade *bb);

#endif /* _DAVRODS_BYTERANGE_H */
