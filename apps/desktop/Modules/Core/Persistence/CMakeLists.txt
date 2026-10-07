include(AedstCompilerWarnings)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

# Linux Secret Service (libsecret) is OPTIONAL: without it the Linux build falls
# back to the 0600 key file. Needs pkg-config and the libsecret-1 dev package
# (Debian/Ubuntu: libsecret-1-dev, Fedora: libsecret-devel) at build time.
option(AEDST_ENABLE_LIBSECRET "Use libsecret (Secret Service) for the master key on Linux when available" ON)
set(AEDST_HAVE_LIBSECRET_TARGET FALSE)
if(AEDST_ENABLE_LIBSECRET AND UNIX)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBSECRET IMPORTED_TARGET libsecret-1)
        if(LIBSECRET_FOUND)
            set(AEDST_HAVE_LIBSECRET_TARGET TRUE)
        endif()
    endif()
    if(NOT AEDST_HAVE_LIBSECRET_TARGET)
        message(STATUS "AedstPersistence: libsecret-1 not found; Linux master key falls back to the key file")
    endif()
endif()

# Thin RAII wrapper over the sqlite3 C API rather than Qt Sql: it keeps
# persistence usable from Qt-free code and GoogleTest, and gives one SQLite
# version everywhere (vcpkg's) instead of whichever QSQLITE plugin Qt shipped.
add_library(AedstPersistence STATIC
    include/aedst/persistence/sqlite_connection.hpp
    include/aedst/persistence/sqlite_statement.hpp
    include/aedst/persistence/master_key_provider.hpp
    include/aedst/persistence/secure_storage.hpp
    include/aedst/persistence/migrator.hpp
    include/aedst/persistence/token_store.hpp
    src/key_file.hpp
    src/key_file.cpp
    src/master_key_provider.cpp
    src/dpapi_master_key_provider.cpp
    src/libsecret_backend.cpp
    src/migrator.cpp
    src/sqlite_connection.cpp
    src/sqlite_statement.cpp
    src/secure_storage.cpp
    src/token_store.cpp
)
add_library(Aedst::Persistence ALIAS AedstPersistence)

target_include_directories(AedstPersistence PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# sqlite3/OpenSSL types never appear in public headers (forward-declared), so
# they stay PRIVATE and don't leak into features.
target_link_libraries(AedstPersistence
    PUBLIC
        Aedst::Core
    PRIVATE
        SQLite::SQLite3
        OpenSSL::Crypto
)

if(WIN32)
    # DPAPI (CryptProtectData / CryptUnprotectData)
    target_link_libraries(AedstPersistence PRIVATE Crypt32)
endif()

if(AEDST_HAVE_LIBSECRET_TARGET)
    target_link_libraries(AedstPersistence PRIVATE PkgConfig::LIBSECRET)
    target_compile_definitions(AedstPersistence PRIVATE AEDST_HAVE_LIBSECRET=1)
endif()

aedst_set_warnings(AedstPersistence)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
