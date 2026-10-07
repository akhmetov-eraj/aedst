find_package(GTest REQUIRED)
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Gui Test)

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

# Image downloads against a loopback server: delivery, and cancellation before and during the transfer (scrolled-away
# delegates must not keep downloading or decoding).
add_executable(AedstMediaImageFetcherTests media_image_fetcher_test.cpp)
target_link_libraries(AedstMediaImageFetcherTests PRIVATE Aedst::Media Aedst::Testing Qt6::Test GTest::gtest)
aedst_set_warnings(AedstMediaImageFetcherTests)
add_test(NAME AedstMedia.ImageFetcherTest COMMAND AedstMediaImageFetcherTests)
set_tests_properties(AedstMedia.ImageFetcherTest PROPERTIES
    LABELS "unit"
    TIMEOUT 120
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
