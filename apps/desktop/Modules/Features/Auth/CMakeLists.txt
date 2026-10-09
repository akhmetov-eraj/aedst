include(AedstCompilerWarnings)

find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Quick)

# LoginView.qml imports `Aedst.Connectivity` (Core) and `Aedst.Feature.Auth`; both are
# declared as QML_SINGLETONs (qt_add_qml_module below) and supplied by App at startup (SingletonSlot),
# so this target needs no C++ link to Aedst::Connectivity.

# BOUNDARY RULE: a feature links Core targets only — never Aedst::Feature::*.
# Cross-feature navigation goes through Aedst::Navigation's route names
# (aedst::navigation::routes), and App/ is the only target that links every
# feature. Because each feature's include root is its own directory, an
# accidental #include of another feature's header fails to compile here — the
# CMake link graph *is* the boundary check (docs/architecture-desktop.md).
add_library(AedstFeatureAuth STATIC
    Domain/session.hpp
    Domain/auth_repository.hpp
    Domain/browser_sign_in_use_case.hpp
    Domain/browser_sign_in_use_case.cpp
    Domain/login_use_case.hpp
    Domain/login_use_case.cpp
    Domain/logout_use_case.hpp
    Domain/logout_use_case.cpp
    Domain/refresh_token_use_case.hpp
    Domain/refresh_token_use_case.cpp
    Data/auth_remote_data_source.hpp
    Data/connect_auth_remote_data_source.hpp
    Data/connect_auth_remote_data_source.cpp
    Data/local_token_store_adapter.hpp
    Data/local_token_store_adapter.cpp
    Data/connect_auth_repository.hpp
    Data/connect_auth_repository.cpp
    Data/oidc_auth_repository.hpp
    Data/oidc_auth_repository.cpp
    Presentation/login_view_model.hpp
    Presentation/login_view_model.cpp
    Navigation/auth_route.hpp
    Navigation/auth_destinations.hpp
    Navigation/auth_destinations.cpp
    # Listed so lupdate (qt_add_translations below) scans its qsTr() strings; the file itself belongs to the
    # AedstFeatureAuthViews QML module below (ahead-of-time compiled).
    Presentation/LoginView.qml
)
add_library(Aedst::Feature::Auth ALIAS AedstFeatureAuth)

# Declarative QML singletons (QML_ELEMENT + QML_SINGLETON, Aedst::QmlSupport); the module URI is the import name QML uses.
# Only App links AedstFeatureAuthplugin, so QML test executables keep registering their fakes under this URI.
target_link_libraries(AedstFeatureAuth PUBLIC Aedst::QmlSupport)
qt_add_qml_module(AedstFeatureAuth URI Aedst.Feature.Auth VERSION 1.0 RESOURCE_PREFIX /qt/qml)

target_include_directories(AedstFeatureAuth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# PUBLIC only where this feature's headers expose the type: Core (Outcome/Error
# in every Domain signature), Network (LocalTokenStoreAdapter implements
# network::TokenProvider), Contract (data sources speak the generated Protobuf
# messages), Navigation
# (AuthRoute builds a navigation::Route). Everything else is an implementation
# detail and stays PRIVATE.
target_link_libraries(AedstFeatureAuth
    PUBLIC
        Aedst::Core
        Aedst::Network
        Aedst::Contract
        Aedst::Navigation
        Aedst::Plugin
        Qt6::Core
    PRIVATE
        Aedst::Persistence
        Aedst::Oidc
        Aedst::Analytics
        Aedst::DesignSystem
        Qt6::Quick
)

# The sign-in screen is the first thing every launch renders: it is a static QML module so qmlcachegen compiles it
# ahead of time (plain qt_add_resources would parse it at every start, docs/performance.md). The URI differs from
# Aedst.Feature.Auth, which declares LoginViewModel as a QML_SINGLETON. Whoever links the feature (App, the QML tests)
# gets the plugin through the INTERFACE link below, so the resources are not dropped by the linker.
set_source_files_properties(Presentation/LoginView.qml PROPERTIES QT_RESOURCE_ALIAS LoginView.qml)
qt_add_qml_module(AedstFeatureAuthViews
    STATIC
    URI Aedst.Feature.Auth.Views
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES Presentation/LoginView.qml
    DEPENDENCIES QtQuick AedstDesignSystem
)
target_link_libraries(AedstFeatureAuth INTERFACE AedstFeatureAuthViewsplugin)

# Per-module Qt Linguist catalog: .ts sources are hand-maintained (and kept in
# sync with qsTr()/tr() call sites via `lupdate`), .qm binaries are generated
# at build time by `lrelease` — Qt's own i18n toolchain, not a hand-rolled
# loader (docs/content/library-first.md).
qt_add_translations(AedstFeatureAuth
    TS_FILES
        i18n/auth_en.ts
        i18n/auth_ru.ts
        i18n/auth_ar.ts
        i18n/auth_tg.ts
        i18n/auth_uz.ts
    LUPDATE_TARGET update_auth_translations
    LRELEASE_TARGET release_auth_translations
)

aedst_set_warnings(AedstFeatureAuth)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
