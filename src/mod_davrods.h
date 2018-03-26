/**
 * \file
 * \brief     Davrods main module file.
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
#ifndef _MOD_DAVRODS_H
#define _MOD_DAVRODS_H

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include <httpd.h>
#include <http_config.h>
#include <http_log.h>
#include <apr_strings.h>
#include <mod_dav.h>

module AP_MODULE_DECLARE_DATA davrods_module;

#if defined(DAVRODS_DEBUG_DESPERATE) || defined(DAVRODS_DEBUG_VERY_DESPERATE)

    #define _TOSTR(x) #x
    #define _LOCSTR(file, line) file ":" _TOSTR(line)

    // Quick / dirty debugging output to stderr (this usually ends up in apache's error log).
    #define WHISPER(...) fprintf(stderr, "[davrods-debug] " _LOCSTR(__FILE__, __LINE__) ": " __VA_ARGS__)

    #ifdef DAVRODS_DEBUG_VERY_DESPERATE

        // printf debugging, but better.
        #define PING() fprintf(stderr, "[%5d] " _LOCSTR(__FILE__, __LINE__) " %s()\n", getpid(), __func__)

    #else /* DAVRODS_DEBUG_VERY_DESPERATE */

        #define PING()

    #endif

#else /* defined(DAVRODS_DEBUG_DESPERATE) || defined(DAVRODS_DEBUG_VERY_DESPERATE) */

    #define WHISPER(...)
    #define PING()

#endif

#ifndef DAVRODS_PROVIDER_NAME
#define DAVRODS_PROVIDER_NAME "davrods"
#endif /* DAVRODS_PROVIDER_NAME */

#ifndef DAVRODS_CONFIG_PREFIX
#define DAVRODS_CONFIG_PREFIX DAVRODS_PROVIDER_NAME
#endif /* DAVRODS_CONFIG_PREFIX */

#endif /* _MOD_DAVRODS_H */
