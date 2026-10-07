#pragma once

// Scope cut: Qt Network (ConnectClient) is the only transport today, and nothing
// includes this header yet.
//
// Boost.Asio is available as an opt-in dependency (vcpkg feature `asio`,
// CMake option AEDST_ENABLE_ASIO) for the first need Qt Network covers
// poorly - e.g. a raw TCP/UDP or custom-framed socket for a realtime/streaming
// module (live updates over a long-lived socket, media streaming), or bulk
// async I/O kept off the GUI event loop. When that lands, own one io_context on
// a dedicated thread here and hand results back to QObjects with
// QMetaObject::invokeMethod(target, ..., Qt::QueuedConnection); never block
// the Qt event loop on Asio or vice versa.

#if defined(AEDST_WITH_BOOST_ASIO)
#include <boost/asio/io_context.hpp>

namespace aedst::network::asio {

using IoContext = boost::asio::io_context;

} // namespace aedst::network::asio
#endif
