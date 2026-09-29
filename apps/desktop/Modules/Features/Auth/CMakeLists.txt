include(AedstCompilerWarnings)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Quick)

# BOUNDARY RULE: a feature links Core targets only — never Aedst::Feature::*.
# Cross-feature navigation goes through Aedst::Navigation's route names
# (aedst::navigation::routes), and App/ is the only target that links every
# feature. Because each feature's include root is its own directory, an
# accidental #include of another feature's header fails to compile here — the
# CMake link graph *is* the boundary check (docs/architecture-desktop.md).
add_library(AedstFeatureAuth STATIC
    Domain/session.hpp
    Domain/auth_repository.hpp
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
    Data/auth_repository_impl.hpp
    Data/auth_repository_impl.cpp
    Presentation/login_view_model.hpp
    Presentation/login_view_model.cpp
    Navigation/auth_route.hpp
    Navigation/auth_destinations.hpp
    Navigation/auth_destinations.cpp
)
add_library(Aedst::Feature::Auth ALIAS AedstFeatureAuth)

target_include_directories(AedstFeatureAuth PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# PUBLIC only where this feature's headers expose the type: Core (Outcome/Error
# in every Domain signature), Network (LocalTokenStoreAdapter implements
# network::TokenProvider; data sources speak network DTOs), Navigation
# (AuthRoute builds a navigation::Route). Everything else is an implementation
# detail and stays PRIVATE.
target_link_libraries(AedstFeatureAuth
    PUBLIC
        Aedst::Core
        Aedst::Network
        Aedst::Navigation
        Qt6::Core
    PRIVATE
        Aedst::Persistence
        Aedst::Analytics
        Aedst::DesignSystem
        Qt6::Quick
)

qt_add_resources(AedstFeatureAuth aedst_feature_auth_qml
    PREFIX "/aedst/feature/auth"
    BASE Presentation
    FILES Presentation/LoginView.qml
)

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
)

aedst_set_warnings(AedstFeatureAuth)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
