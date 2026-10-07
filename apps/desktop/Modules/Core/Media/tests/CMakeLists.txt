find_package(GTest REQUIRED)
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Gui)

# Pure logic + Qt image codecs: variant selection, image:// id encoding (incl. the
# no-local-files guard), compression sizing/format, list model bookkeeping.
# Network upload and video grabbing need a live object store / codecs: covered by
# the manual checklist in docs/adr/0022-telemetry-media-notifications.md.
add_executable(AedstMediaTests media_test.cpp)
target_link_libraries(AedstMediaTests PRIVATE Aedst::Media Qt6::Gui GTest::gtest)
aedst_set_warnings(AedstMediaTests)

add_test(NAME AedstMedia.MediaTest COMMAND AedstMediaTests)
set_tests_properties(AedstMedia.MediaTest PROPERTIES
    LABELS "unit"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
