include_guard(GLOBAL)

# Linux layout of the bundled Qt (AEDST_DEPLOY_QT=ON, the DEB / RPM / TGZ path).
#
# Qt's deploy script defaults put Qt's libraries into <prefix>/lib, its plugins into <prefix>/plugins and its QML
# modules into <prefix>/qml. With CPACK_PACKAGING_INSTALL_PREFIX=/usr that would drop /usr/lib/libQt6*.so* (shadowing
# or conflicting with the distribution's own Qt packages) and stray /usr/plugins, /usr/qml directories on the system.
# Everything Qt-shaped goes under ONE private directory instead:
#
#     <libdir>/aedst-desktop/            libQt6*.so*, ICU, ...
#     <libdir>/aedst-desktop/plugins/    Qt plugins
#     <libdir>/aedst-desktop/qml/        QML modules
#     <libdir>/aedst-desktop/translations/  qtbase_<lang>.qm, qtdeclarative_<lang>.qm (TranslationLoader)
#
# and the executable finds it through a relative RUNPATH ($ORIGIN/../<libdir>/aedst-desktop) plus a qt.conf next to it
# (plugins/QML/translations are not found through RUNPATH). scripts/verify-install-linux.sh proves that nothing
# Qt-shaped is installed outside this directory. UNVERIFIED until the first Linux package build: the QT_DEPLOY_* variable
# names follow the Qt 6 deploy documentation, and whether Qt's own script also writes a qt.conf (this file's wins or is
# identical) is unchecked.
#
#   aedst_configure_linux_bundle(<target>)     call before qt_generate_deploy_qml_app_script()

function(aedst_configure_linux_bundle target)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        return()
    endif()

    set(bundle "${CMAKE_INSTALL_LIBDIR}/aedst-desktop")
    set(QT_DEPLOY_LIB_DIR "${bundle}" PARENT_SCOPE)
    set(QT_DEPLOY_PLUGINS_DIR "${bundle}/plugins" PARENT_SCOPE)
    set(QT_DEPLOY_QML_DIR "${bundle}/qml" PARENT_SCOPE)
    set(QT_DEPLOY_TRANSLATIONS_DIR "${bundle}/translations" PARENT_SCOPE)

    # bin -> bundle, computed from the install layout so a multiarch libdir (lib/x86_64-linux-gnu) works.
    file(RELATIVE_PATH from_bin_to_bundle "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}/aedst-desktop")
    set_target_properties(${target} PROPERTIES
        INSTALL_RPATH "\$ORIGIN/${from_bin_to_bundle}"
        INSTALL_RPATH_USE_LINK_PATH FALSE)

    set(qt_conf "${CMAKE_CURRENT_BINARY_DIR}/qt.conf")
    file(WRITE "${qt_conf}"
"[Paths]
Prefix = .
Libraries = ${from_bin_to_bundle}
Plugins = ${from_bin_to_bundle}/plugins
QmlImports = ${from_bin_to_bundle}/qml
Translations = ${from_bin_to_bundle}/translations
")
    install(FILES "${qt_conf}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endfunction()
