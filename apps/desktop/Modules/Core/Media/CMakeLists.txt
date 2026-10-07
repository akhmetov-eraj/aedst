include(AedstCompilerWarnings)

# Multimedia is an add-on Qt module (aqt: -m qtmultimedia; CI installs it).
find_package(Qt6 ${AEDST_QT_MINIMUM} REQUIRED COMPONENTS Core Gui Network Qml Quick Multimedia)

# Shared media building blocks every feature reuses without linking another
# feature (Features/Media = library screen, Features/Tasks = attachments):
#   - MediaRemoteDataSource: aedst.media.v1.MediaService over ConnectClient
#   - MediaUploadController: compress -> CreateUpload -> presigned PUT -> CompleteUpload
#   - PresignedUploader: QNetworkAccessManager PUT with uploadProgress / cancel
#   - compressImage: QImageReader/QImageWriter downscale + quality, EXIF stripped
#   - MediaImageProvider: async image://media provider with memory + disk cache
#   - VideoFrameGrabber: QMediaPlayer + QVideoSink first-frame thumbnails
#   - MediaListModel: QAbstractListModel over the generated Media messages
# and the `Aedst.Media` QML module (MediaImage, VideoPlayer, MediaUploadPanel,
# MediaThumbnail, MediaGrid).
set(AEDST_MEDIA_QML_FILES
    qml/AedstMedia/MediaImage.qml
    qml/AedstMedia/VideoPlayer.qml
    qml/AedstMedia/MediaUploadPanel.qml
    qml/AedstMedia/MediaThumbnail.qml
    qml/AedstMedia/MediaGrid.qml
)
foreach(qml_file IN LISTS AEDST_MEDIA_QML_FILES)
    get_filename_component(qml_file_name ${qml_file} NAME)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS ${qml_file_name})
endforeach()

qt_add_qml_module(AedstMedia
    STATIC
    URI Aedst.Media
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    SOURCES
        include/aedst/media/image_compressor.hpp
        include/aedst/media/media_image_provider.hpp
        include/aedst/media/media_list_model.hpp
        include/aedst/media/media_remote_data_source.hpp
        include/aedst/media/media_sources.hpp
        include/aedst/media/media_upload_controller.hpp
        include/aedst/media/presigned_uploader.hpp
        include/aedst/media/video_frame_grabber.hpp
        src/image_compressor.cpp
        src/media_image_provider.cpp
        src/media_list_model.cpp
        src/media_remote_data_source.cpp
        src/media_sources.cpp
        src/media_upload_controller.cpp
        src/presigned_uploader.cpp
        src/video_frame_grabber.cpp
    QML_FILES ${AEDST_MEDIA_QML_FILES}
    # Imported by the QML files; declared so qmllint/qmlcachegen resolve their types.
    DEPENDENCIES
        QtQuick
        QtMultimedia
        AedstDesignSystem
)
add_library(Aedst::Media ALIAS AedstMedia)

# QML_ELEMENT headers are found by moc through this include root.
target_include_directories(AedstMedia PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

# PUBLIC: headers expose generated media messages (Contract), ConnectClient-based
# data sources (Network), QObject/QAbstractListModel/QQuickAsyncImageProvider types.
target_link_libraries(AedstMedia
    PUBLIC
        Aedst::Core
        Aedst::Network
        Aedst::Contract
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
    PRIVATE
        Qt6::Multimedia
        Aedst::Logging
        Aedst::DesignSystem
)
# Same static-QML-module rule as Aedst::DesignSystem: consumers get the plugin.
target_link_libraries(AedstMedia INTERFACE AedstMediaplugin)

aedst_set_warnings(AedstMedia)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
