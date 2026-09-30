# CPACK_PRE_BUILD_SCRIPTS hook (CMake >= 3.19): runs after the install tree is staged
# for a generator and before it is packaged. Re-signs the staged .app with the hardened
# runtime when APPLE_SIGNING_IDENTITY is present; otherwise does nothing.
#
# UNVERIFIED: that CPACK_TEMPORARY_DIRECTORY is the variable visible to this script for
# both DragNDrop and productbuild on the CMake version CI pins. Both it and
# CPACK_TEMPORARY_INSTALL_DIRECTORY are searched.

if("$ENV{APPLE_SIGNING_IDENTITY}" STREQUAL "")
    message(STATUS "AEDST: APPLE_SIGNING_IDENTITY not set; leaving the staged .app unsigned")
    return()
endif()

set(_aedst_stage_dirs "")
foreach(v CPACK_TEMPORARY_DIRECTORY CPACK_TEMPORARY_INSTALL_DIRECTORY)
    if(DEFINED ${v} AND EXISTS "${${v}}")
        list(APPEND _aedst_stage_dirs "${${v}}")
    endif()
endforeach()
list(REMOVE_DUPLICATES _aedst_stage_dirs)

set(_aedst_apps "")
foreach(d IN LISTS _aedst_stage_dirs)
    file(GLOB _found LIST_DIRECTORIES true "${d}/*.app" "${d}/*/*.app")
    list(APPEND _aedst_apps ${_found})
endforeach()
list(REMOVE_DUPLICATES _aedst_apps)

if(NOT _aedst_apps)
    message(FATAL_ERROR "AEDST: APPLE_SIGNING_IDENTITY is set but no .app was found under: ${_aedst_stage_dirs}")
endif()

foreach(app IN LISTS _aedst_apps)
    message(STATUS "AEDST: signing ${app}")
    execute_process(
        COMMAND bash "${CPACK_AEDST_SIGN_SCRIPT}" sign-app "${app}"
        RESULT_VARIABLE _rc)
    if(NOT _rc EQUAL 0)
        message(FATAL_ERROR "AEDST: signing ${app} failed (${_rc})")
    endif()
endforeach()
