include(AedstCompilerWarnings)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

# Thin RAII wrapper over the sqlite3 C API rather than Qt Sql: it keeps
# persistence usable from Qt-free code and GoogleTest, and gives one SQLite
# version everywhere (Conan's) instead of whichever QSQLITE plugin Qt shipped.
add_library(AedstPersistence STATIC
    include/aedst/persistence/sqlite_connection.hpp
    include/aedst/persistence/sqlite_statement.hpp
    include/aedst/persistence/secure_storage.hpp
    include/aedst/persistence/token_store.hpp
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
        OpenSSL::SSL
        OpenSSL::Crypto
)

if(WIN32)
    # DPAPI (CryptProtectData / CryptUnprotectData)
    target_link_libraries(AedstPersistence PRIVATE Crypt32)
endif()

aedst_set_warnings(AedstPersistence)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
