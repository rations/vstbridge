// vst3bridge: A modern Wine VST3 bridge for Linux
// Copyright (C) 2026
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2 of the License.

#include <windows.h>
#include <iostream>
#include <string>

#include "../common/protocol.h"
#include "../common/socket.h"

namespace vst3bridge {

// Wine Host Main Entry Point
// This executable runs under Wine and hosts the actual VST3 plugin

int RunHost(const std::string& socket_path) {
    // TODO:
    // 1. Create server socket
    // 2. Listen for connections from native plugin
    // 3. Handle handshake
    // 4. Enter message processing loop
    // 5. Cleanup on exit
    
    std::cout << "vst3bridge-host starting..." << std::endl;
    
    ServerSocket server;
    if (!server.Listen(socket_path)) {
        std::cerr << "Failed to create server socket" << std::endl;
        return 1;
    }
    
    std::cout << "Listening on: " << socket_path << std::endl;
    
    // Accept one connection
    auto client = server.Accept();
    if (!client) {
        std::cerr << "Failed to accept connection" << std::endl;
        return 1;
    }
    
    std::cout << "Client connected" << std::endl;
    
    // TODO: Implement message handling loop
    
    return 0;
}

} // namespace vst3bridge

// Windows entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                   LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line
    std::string socket_path = lpCmdLine;
    
    if (socket_path.empty()) {
        // Generate default socket path
        socket_path = "/tmp/vst3bridge_" + std::to_string(GetCurrentProcessId());
    }
    
    return vst3bridge::RunHost(socket_path);
}

// Console entry point (for debugging)
int main(int argc, char* argv[]) {
    std::string socket_path;
    
    if (argc > 1) {
        socket_path = argv[1];
    } else {
        socket_path = "/tmp/vst3bridge_" + std::to_string(GetCurrentProcessId());
    }
    
    return vst3bridge::RunHost(socket_path);
}
