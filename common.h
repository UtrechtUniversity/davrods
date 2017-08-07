/**
 * \file
 * \brief     Common includes, functions and variables.
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
#ifndef _DAVRODS_COMMON_H_
#define _DAVRODS_COMMON_H_

#include "mod_davrods.h"

// I'm not sure why, but the format string apr.h generates on my machine (%lu)
// causes compiler warnings. It seems that gcc wants us to use 'llu' instead,
// however the apr sprintf function does not support the long-long notation.
// -Wformat warnings related to %lu parameters can probably be ignored.
#define DAVRODS_SIZE_T_FMT APR_SIZE_T_FMT
//#define DAVRODS_SIZE_T_FMT "llu"

/**
 * \brief Get the iRODS error message for the given iRODS status code.
 *
 * \param rods_error_code
 *
 * \return an error description provided by iRODS
 */
char *get_rods_error_msg(int rods_error_code);

void davrods_dav_register(apr_pool_t *p);

#endif /* _DAVRODS_COMMON_H_ */
