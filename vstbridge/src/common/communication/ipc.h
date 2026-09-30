// vstbridge: a Wine VST/CLAP plugin bridge
// Copyright (C) 2020-2024 Robbert van der Helm
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

// The Unix domain socket types used for communication between the native
// plugin and the Wine plugin host. These are Asio's local stream sockets,
// except in the PE plugin host (aarch64), which has its own implementation
// backed by native Linux sockets in its unixlib.

#ifdef VSTBRIDGE_PE_HOST
#include "../../wine-host/unixlib/ipc-socket.h"
#else

#ifdef __WINE__
#include "../../wine-host/use-linux-asio.h"
#endif
#include <asio/local/stream_protocol.hpp>
#include <ghc/filesystem.hpp>

namespace ipc {

using socket = asio::local::stream_protocol::socket;
using acceptor = asio::local::stream_protocol::acceptor;
using endpoint = asio::local::stream_protocol::endpoint;

inline void create_endpoint_directory(const endpoint& endpoint) {
    ghc::filesystem::create_directories(
        ghc::filesystem::path(endpoint.path()).parent_path());
}

inline void remove_endpoint(const endpoint& endpoint) {
    ghc::filesystem::remove(endpoint.path());
}

inline void remove_directory(const std::string& path) {
    ghc::filesystem::remove_all(path);
}

}  // namespace ipc

#endif
