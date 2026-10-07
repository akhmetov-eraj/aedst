include_guard(GLOBAL)

# Single source of truth for the Qt version: qt-version.txt (e.g. "6.12.0").
# Consumers:
#   - this file  -> AEDST_QT_VERSION / AEDST_QT_MINIMUM (find_package minimum, major.minor)
#   - CI / dev   -> `aqt install-qt ... "$(cat apps/desktop/qt-version.txt)" ...`
#   - README     -> same one-liner
# Bump Qt by editing that one file.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/qt-version.txt" AEDST_QT_VERSION LIMIT_COUNT 1)
string(STRIP "${AEDST_QT_VERSION}" AEDST_QT_VERSION)
if(NOT AEDST_QT_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "qt-version.txt must contain a single MAJOR.MINOR.PATCH version, got '${AEDST_QT_VERSION}'")
endif()
set(AEDST_QT_MINIMUM "${CMAKE_MATCH_1}.${CMAKE_MATCH_2}")

option(AEDST_ENFORCE_QT_VERSION "Fail configure when the found Qt's major.minor differs from qt-version.txt" ON)

# Call after find_package(Qt6 ...).
function(aedst_check_qt_version)
    if("${Qt6_VERSION}" VERSION_EQUAL "${AEDST_QT_VERSION}")
        message(STATUS "AEDST: Qt ${Qt6_VERSION} (matches qt-version.txt)")
        return()
    endif()
    string(REGEX MATCH "^[0-9]+\\.[0-9]+" found_minor "${Qt6_VERSION}")
    if(NOT found_minor VERSION_EQUAL AEDST_QT_MINIMUM AND AEDST_ENFORCE_QT_VERSION)
        message(FATAL_ERROR "Found Qt ${Qt6_VERSION} but qt-version.txt pins ${AEDST_QT_VERSION}. "
                            "Install the pinned version (see README) or pass -DAEDST_ENFORCE_QT_VERSION=OFF.")
    endif()
    message(WARNING "AEDST: Qt ${Qt6_VERSION} differs from pinned ${AEDST_QT_VERSION}; CI builds use the pinned version.")
endfunction()
