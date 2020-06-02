cmake_minimum_required(VERSION 2.8.12)

project(davrods C)

set(IRODSRT_VERSION "4.2.7" CACHE STRING "iRODS client library version")

set(DAVRODS_FEATURE     "1.4.2")
set(DAVRODS_VERSION     "${IRODSRT_VERSION}_${DAVRODS_FEATURE}")
set(DAVRODS_VERSION_DEB "${IRODSRT_VERSION}-${DAVRODS_FEATURE}")

find_program(APXS apxs DOC "Apache/HTTPD extension tool location")
if(NOT APXS)
    message(FATAL_ERROR "Could not find apxs utility - make sure the apache/httpd dev package is installed")
endif()

# Get platform-dependent HTTPD include paths.
execute_process(COMMAND ${APXS} -q exp_includedir
                OUTPUT_VARIABLE HTTPD_INCLUDE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${APXS} -q APR_INCLUDEDIR
                OUTPUT_VARIABLE APR_INCLUDE_DIR   OUTPUT_STRIP_TRAILING_WHITESPACE)

# Get installation paths (local to the build system).
execute_process(COMMAND ${APXS} -q exp_libexecdir
                OUTPUT_VARIABLE HTTPD_BUILDSYS_MODULE_DIR  OUTPUT_STRIP_TRAILING_WHITESPACE)

# Getting the iRODS include path from the iRODS CMake module results in errors
# if the irods-externals clang version is not installed. So, specify it
# manually instead.
set(IRODS_INCLUDE_DIR "/usr/include/irods" CACHE STRING "iRODS include directory")

if(IS_DIRECTORY /etc/httpd/conf.modules.d)
    # This looks like CentOS7's httpd, we know where to put our files on install.
    set(SYSTEM_LOOKS_LIKE "CentOS7")
elseif(IS_DIRECTORY /etc/apache2/mods-available)
    # This looks like Debian/Ubuntu, we know where to put our files on install.
    # Thanks to @holtgrewe for the initial porting work.
    set(SYSTEM_LOOKS_LIKE "Debian")

    # Debian notes, postinstall:
    #
    # a2enmod davrods
    # a2enmod dav
    # apache2ctl restart
else()
    set(SYSTEM_LOOKS_LIKE "--Unknown--")
    message(WARNING "
Davrods' build system currently only supports the cmake 'install' target on CentOS7 and Debian-like systems.
If you are running CentOS or similar, make sure httpd is installed before running cmake: This build system requires certain HTTPD directories to be in place.
If you are running a different Linux distribution or if your HTTPD configuration layout differs otherwise, you can install Davrods manually after building. See the instructions in README.md.")
endif()

include_directories(${IRODS_INCLUDE_DIR}
                    ${HTTPD_INCLUDE_DIR}
                      ${APR_INCLUDE_DIR})

link_libraries(irods_client)

add_compile_options(-Wall
                    -Wextra
                    -pedantic
                    -Wno-unused-parameter
                    -Wno-missing-field-initializers
                    -Wno-format # Due to APR/iRODS format specifier oddities.
                    -std=c99
                    -g3
                    -ggdb
                    -O2
                    -fexceptions
                    -DLINUX
                    -D_REENTRANT
                    -D_GNU_SOURCE
                    -D_FORTIFY_SOURCE=2
                    -fstack-protector-strong
                    --param=ssp-buffer-size=4
                    -DDAVRODS_ENABLE_PROVIDER_NOLOCKS
                    -DDAVRODS_ENABLE_PROVIDER_LOCALLOCK)

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,relro,-z,now")

set(SOURCES
    src/mod_davrods.c
    src/auth.c
    src/common.c
    src/config.c
    src/prop.c
    src/propdb.c
    src/repo.c
    src/listing.c
    src/lock_local.c
    src/byterange.c)

add_library(mod_davrods SHARED ${SOURCES})

# Remove "lib" prefix from module SO file.
set_property(TARGET mod_davrods PROPERTY PREFIX "")

# Enable OS-dependent installation targets
if(SYSTEM_LOOKS_LIKE STREQUAL "CentOS7")
    install(TARGETS     mod_davrods
            DESTINATION ${HTTPD_BUILDSYS_MODULE_DIR})

    install(FILES       aux/rpm/davrods.conf
            DESTINATION /etc/httpd/conf.modules.d
            RENAME      10-davrods.conf)

    install(FILES       aux/rpm/davrods-vhost.conf
                        aux/rpm/davrods-anonymous-vhost.conf
            DESTINATION /etc/httpd/conf.d/)

    install(FILES       aux/common/irods_environment.json
            DESTINATION /etc/httpd/irods/)

    install(FILES       aux/common/listing/head.html
                        aux/common/listing/header.html
                        aux/common/listing/footer.html
                        aux/common/listing/README.md
            DESTINATION /etc/httpd/irods/)

    install(FILES       README.md README.advanced.md COPYING COPYING.LESSER changelog.txt
            DESTINATION /usr/share/doc/davrods-${DAVRODS_VERSION}/)

    install(DIRECTORY
            DESTINATION /var/lib/davrods)

elseif(SYSTEM_LOOKS_LIKE STREQUAL "Debian")
    install(TARGETS     mod_davrods
            DESTINATION ${HTTPD_BUILDSYS_MODULE_DIR})

    install(FILES       aux/deb/davrods.conf
            DESTINATION /etc/apache2/mods-available
            RENAME      davrods.load)

    install(FILES       aux/deb/davrods-vhost.conf
                        aux/deb/davrods-anonymous-vhost.conf
            DESTINATION /etc/apache2/sites-available/)

    install(FILES       aux/common/irods_environment.json
            DESTINATION /etc/apache2/irods/)

    install(FILES       aux/common/listing/head.html
                        aux/common/listing/header.html
                        aux/common/listing/footer.html
                        aux/common/listing/README.md
            DESTINATION /etc/apache2/irods/)

    install(FILES       README.md README.advanced.md COPYING COPYING.LESSER changelog.txt
            DESTINATION /usr/share/doc/davrods-${DAVRODS_VERSION}/)

    install(DIRECTORY
            DESTINATION /var/lib/davrods)
endif()

if(SYSTEM_LOOKS_LIKE STREQUAL "CentOS7")
    set(CPACK_MONOLITHIC_INSTALL 1)
    set(CPACK_CMAKE_GENERATOR "Unix Makefiles")
    set(CPACK_GENERATOR "RPM")
    set(CPACK_PACKAGE_NAME "davrods")
    set(CPACK_PACKAGE_VENDOR "Utrecht University <fbyoda@uu.nl>")
    set(CPACK_PACKAGE_CONTACT "Utrecht University <fbyoda@uu.nl>")
    set(CPACK_PACKAGE_VERSION "${DAVRODS_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/aux/common/description.txt")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A DAV level 2 compliant Apache interface to iRODS")

    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.LESSER")
    set(CPACK_RESOURCE_FILE_README  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

    set(CPACK_RPM_PACKAGE_RELEASE "1")
    set(CPACK_RPM_PACKAGE_LICENSE "LGPLv3+")
    set(CPACK_RPM_PACKAGE_REQUIRES "httpd >= 2.4, irods-runtime = ${IRODSRT_VERSION}")
    set(CPACK_RPM_PACKAGE_URL "https://github.com/UtrechtUniversity/davrods")
    set(CPACK_RPM_CHANGELOG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/changelog.txt")
    set(CPACK_RPM_PACKAGE_AUTOREQ 0)
    set(CPACK_RPM_PACKAGE_AUTOPROV 0)
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/aux/rpm/postinst.sh")

    set(CPACK_RPM_USER_FILELIST
        "%doc /usr/share/doc/davrods-${DAVRODS_VERSION}/README.md"
        "%doc /usr/share/doc/davrods-${DAVRODS_VERSION}/README.advanced.md"
        "%doc /usr/share/doc/davrods-${DAVRODS_VERSION}/COPYING"
        "%doc /usr/share/doc/davrods-${DAVRODS_VERSION}/COPYING.LESSER"
        "%doc /usr/share/doc/davrods-${DAVRODS_VERSION}/changelog.txt"
        "%config(noreplace) /etc/httpd/conf.modules.d/10-davrods.conf"
        "%config(noreplace) /etc/httpd/conf.d/davrods-vhost.conf"
        "%config(noreplace) /etc/httpd/conf.d/davrods-anonymous-vhost.conf"
        "%config(noreplace) /etc/httpd/irods/irods_environment.json"
        "%config(noreplace) /etc/httpd/irods/head.html"
        "%config(noreplace) /etc/httpd/irods/header.html"
        "%config(noreplace) /etc/httpd/irods/footer.html"
        "%attr(700,apache,apache) /var/lib/davrods")

    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_RPM_PACKAGE_RELEASE}")

    include(CPack)

elseif(SYSTEM_LOOKS_LIKE STREQUAL "Debian")
    set(CPACK_MONOLITHIC_INSTALL 1)
    set(CPACK_CMAKE_GENERATOR "Unix Makefiles")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "davrods")
    set(CPACK_PACKAGE_VENDOR "Utrecht University <fbyoda@uu.nl>")
    set(CPACK_PACKAGE_CONTACT "Utrecht University <fbyoda@uu.nl>")
    set(CPACK_PACKAGE_VERSION "${DAVRODS_VERSION_DEB}")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/aux/common/description.txt")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A DAV level 2 compliant Apache interface to iRODS")

    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.LESSER")
    set(CPACK_RESOURCE_FILE_README  "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "apache2 (>= 2.4), irods-runtime (= ${IRODSRT_VERSION})")
    set(CPACK_DEBIAN_PACKAGE_SECTION "httpd")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/aux/deb/postinst")

    set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")

    include(CPack)

endif()
