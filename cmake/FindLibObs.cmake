# SPDX-License-Identifier: GPL-2.0-or-later

set(_LIBOBS_HINTS
    "${OBS_ROOT}"
    "$ENV{OBS_ROOT}"
    "${LibObs_ROOT}"
)

find_path(LIBOBS_INCLUDE_DIR
    NAMES obs.h
    PATHS ${_LIBOBS_HINTS}
    PATH_SUFFIXES include include/obs libobs
)

find_path(LIBOBS_CONFIG_INCLUDE_DIR
    NAMES obsconfig.h
    PATHS ${_LIBOBS_HINTS}
    PATH_SUFFIXES config
)

find_library(LIBOBS_LIBRARY
    NAMES obs libobs
    PATHS ${_LIBOBS_HINTS}
    PATH_SUFFIXES lib lib/64bit bin/64bit libobs/Debug libobs/Release libobs/MinSizeRel libobs/RelWithDebInfo Debug Release MinSizeRel RelWithDebInfo
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibObs
    REQUIRED_VARS LIBOBS_LIBRARY LIBOBS_INCLUDE_DIR LIBOBS_CONFIG_INCLUDE_DIR
)

if(LibObs_FOUND AND NOT TARGET LibObs::LibObs)
    add_library(LibObs::LibObs UNKNOWN IMPORTED)
    set_target_properties(LibObs::LibObs PROPERTIES
        IMPORTED_LOCATION "${LIBOBS_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBOBS_INCLUDE_DIR};${LIBOBS_CONFIG_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(LIBOBS_INCLUDE_DIR LIBOBS_CONFIG_INCLUDE_DIR LIBOBS_LIBRARY)
