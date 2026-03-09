/*
 * Copyright (C) 2026
 * VST3 Bridge - Wine VST3 Host Bridge
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/**
 * @file vst3_plugin.h
 * @brief Native VST3 plugin wrapper
 */

#pragma once

#include "plugin_proxy.h"

namespace vst3bridge {

/**
 * @brief Native VST3 plugin wrapper
 * 
 * Wraps the plugin proxy and provides additional native-side functionality
 * like audio buffer management and GUI handling.
 */
class VST3Plugin {
public:
    /**
     * @brief Construct plugin wrapper
     * @param socket Communication socket
     * @param instance_id Instance ID from Wine host
     */
    VST3Plugin(std::shared_ptr<MessageSocket> socket, uint64_t instance_id);
    
    ~VST3Plugin();

    /**
     * @brief Get the plugin proxy
     * @return Plugin proxy
     */
    PluginProxy* getProxy();

private:
    std::unique_ptr<PluginProxy> proxy_;
};

} // namespace vst3bridge
