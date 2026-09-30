include(AedstCompilerWarnings)

find_package(OpenSSL REQUIRED)
# Qt6::Core is used ONLY for QJsonDocument (manifest/envelope parsing) and
# QByteArray base64; every public header is Qt-free.
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core)

# Release-signing public keys compiled into the binary, as
# "keyId:<64 hex chars>[,keyId2:<64 hex chars>...]" (Ed25519 raw public keys;
# several entries allow key rotation). Left empty on purpose: with no trusted key
# every manifest is rejected (fail closed). CI release builds pass
#   -DAEDST_UPDATE_PUBLIC_KEYS=<keys>
set(AEDST_UPDATE_PUBLIC_KEYS "" CACHE STRING
    "Trusted Ed25519 update-signing public keys: keyId:hex[,keyId:hex...]")

# Secure update *verification* (no download, no install): signed manifest,
# semver + rollback protection, https-only URLs, streaming SHA-256 of artifacts.
add_library(AedstUpdate STATIC
    include/aedst/update/artifact_verifier.hpp
    include/aedst/update/http_fetcher.hpp
    include/aedst/update/manifest.hpp
    include/aedst/update/manifest_verifier.hpp
    include/aedst/update/semver.hpp
    include/aedst/update/update_checker.hpp
    include/aedst/update/update_state_store.hpp
    include/aedst/update/url_validation.hpp
    src/artifact_verifier.cpp
    src/hex.hpp
    src/manifest.cpp
    src/manifest_verifier.cpp
    src/semver.cpp
    src/update_checker.cpp
    src/update_state_store.cpp
    src/url_validation.cpp
)
add_library(Aedst::Update ALIAS AedstUpdate)

target_include_directories(AedstUpdate PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(AedstUpdate PUBLIC cxx_std_23)

# OpenSSL and Qt types never appear in public headers, so both stay PRIVATE.
target_link_libraries(AedstUpdate
    PUBLIC
        Aedst::Core
    PRIVATE
        Qt6::Core
        OpenSSL::Crypto
)

target_compile_definitions(AedstUpdate PRIVATE
    "AEDST_UPDATE_PUBLIC_KEYS=\"${AEDST_UPDATE_PUBLIC_KEYS}\"")

aedst_set_warnings(AedstUpdate)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
