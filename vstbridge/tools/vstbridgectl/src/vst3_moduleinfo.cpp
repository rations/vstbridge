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

#include "vst3_moduleinfo.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace {

// Decode a 32-character hex string into 16 bytes
std::array<uint8_t, 16> decode_hex_uid(const std::string& hex) {
    if (hex.size() != 32)
        throw std::runtime_error("Incorrect UID hex string length: '" + hex + "'");
    std::array<uint8_t, 16> uid{};
    for (size_t i = 0; i < 16; ++i) {
        auto byte_str = hex.substr(i * 2, 2);
        try {
            uid[i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        } catch (...) {
            throw std::runtime_error("Invalid hexadecimal string: '" + hex + "'");
        }
    }
    return uid;
}

// Encode 16 bytes as a 32-character uppercase hex string
std::string encode_hex_uid(const std::array<uint8_t, 16>& uid) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (uint8_t b : uid)
        oss << std::setw(2) << static_cast<unsigned>(b);
    return oss.str();
}

// Switch between COM and non-COM byte orders for a VST3 class UID.
// Swaps: 0↔3, 1↔2, 4↔5, 6↔7
std::array<uint8_t, 16> rewrite_uid_byte_order(const std::array<uint8_t, 16>& uid) {
    auto r = uid;
    std::swap(r[0], r[3]);
    std::swap(r[1], r[2]);
    std::swap(r[4], r[5]);
    std::swap(r[6], r[7]);
    return r;
}

std::string rewrite_cid(const std::string& cid) {
    return encode_hex_uid(rewrite_uid_byte_order(decode_hex_uid(cid)));
}

// moduleinfo.json files are JSON5-style: the VST3 SDK writes a comma after
// every object member and array element, and the format allows comments.
// nlohmann::json 3.11 can skip comments but rejects trailing commas, so this
// removes both, leaving strings untouched. yabridgectl used serde_jsonrc,
// which accepts them.
std::string strip_comments_and_trailing_commas(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    // Index in `out` of a comma that's only kept if a value follows it
    std::optional<size_t> pending_comma;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '"') {
            // Copy the whole string literal, including escaped quotes
            pending_comma.reset();
            const size_t begin = i++;
            while (i < text.size() && text[i] != '"')
                i += text[i] == '\\' ? 2 : 1;
            out.append(text, begin, std::min(i, text.size() - 1) - begin + 1);
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n')
                ++i;
            if (i < text.size())
                out += '\n';
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '*') {
            const size_t end = text.find("*/", i + 2);
            i = end == std::string::npos ? text.size() : end + 1;
            out += ' ';
        } else if (c == ',') {
            pending_comma = out.size();
            out += c;
        } else if (c == '}' || c == ']') {
            if (pending_comma)
                out.erase(*pending_comma, 1);
            pending_comma.reset();
            out += c;
        } else {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
                pending_comma.reset();
            out += c;
        }
    }

    return out;
}

}  // namespace

std::string rewrite_moduleinfo_uid_byte_orders(const std::string& json_text) {
    json doc;
    try {
        doc = json::parse(strip_comments_and_trailing_commas(json_text));
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("Could not parse moduleinfo.json: ") + e.what());
    }

    // Rewrite CIDs in the "Classes" array
    if (doc.contains("Classes") && doc["Classes"].is_array()) {
        for (auto& cls : doc["Classes"]) {
            if (cls.contains("CID") && cls["CID"].is_string())
                cls["CID"] = rewrite_cid(cls["CID"].get<std::string>());
        }
    }

    // Rewrite CIDs in the "Compatibility" array
    if (doc.contains("Compatibility") && doc["Compatibility"].is_array()) {
        for (auto& mapping : doc["Compatibility"]) {
            if (mapping.contains("New") && mapping["New"].is_string())
                mapping["New"] = rewrite_cid(mapping["New"].get<std::string>());
            if (mapping.contains("Old") && mapping["Old"].is_array()) {
                for (auto& cid : mapping["Old"])
                    if (cid.is_string())
                        cid = rewrite_cid(cid.get<std::string>());
            }
        }
    }

    return doc.dump(2);
}
