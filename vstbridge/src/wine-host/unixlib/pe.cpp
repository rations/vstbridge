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

#include <optional>

#include "pe.h"

#include <windows.h>
#include <shellapi.h>
#include <winternl.h>

namespace {

using unixlib_handle_t = uint64_t;

// `wine/include/winternl.h`, `MEMORY_INFORMATION_CLASS`
constexpr ULONG MemoryWineLoadUnixLibByName = 1002;

using NtQueryVirtualMemory_t =
    NTSTATUS(NTAPI*)(HANDLE, PVOID, ULONG, PVOID, SIZE_T, PSIZE_T);
using wine_get_dos_file_name_t = WCHAR*(CDECL*)(LPCSTR);
using wine_get_unix_file_name_t = char*(CDECL*)(LPCWSTR);

unixlib_handle_t unixlib_handle = 0;
wine_get_dos_file_name_t wine_get_dos_file_name_ptr = nullptr;
wine_get_unix_file_name_t wine_get_unix_file_name_ptr = nullptr;

}  // namespace

#ifdef __arm64ec__
// ARM64EC code has to call ntdll's dispatcher without going through the x64
// indirect call checker, so this uses the same trampoline as winecrt0
// (`wine/libs/winecrt0/unix_lib.c`, `__wine_unix_call_arm64ec()`)
extern "C" void* vstbridge_unix_call_dispatcher = nullptr;

extern "C" NTSTATUS __attribute__((naked))
vstbridge_unix_call(unixlib_handle_t, unsigned int, void*) {
    asm(".seh_proc \"#vstbridge_unix_call\"\n\t"
        ".seh_endprologue\n\t"
        "adrp x16, vstbridge_unix_call_dispatcher\n\t"
        "ldr x16, [x16, #:lo12:vstbridge_unix_call_dispatcher]\n\t"
        "br x16\n\t"
        ".seh_endproc");
}

static bool init_dispatcher(HMODULE ntdll) {
    auto* dispatcher = reinterpret_cast<void**>(
        GetProcAddress(ntdll, "__wine_unix_call_dispatcher_arm64ec"));
    if (!dispatcher) {
        return false;
    }

    vstbridge_unix_call_dispatcher = *dispatcher;
    return true;
}
#else
using unix_call_dispatcher_t = NTSTATUS(WINAPI*)(unixlib_handle_t,
                                                 unsigned int,
                                                 void*);
static unix_call_dispatcher_t unix_call_dispatcher = nullptr;

static NTSTATUS vstbridge_unix_call(unixlib_handle_t handle,
                                    unsigned int code,
                                    void* args) {
    return unix_call_dispatcher(handle, code, args);
}

static bool init_dispatcher(HMODULE ntdll) {
    auto* dispatcher = reinterpret_cast<unix_call_dispatcher_t*>(
        GetProcAddress(ntdll, "__wine_unix_call_dispatcher"));
    if (!dispatcher) {
        return false;
    }

    unix_call_dispatcher = *dispatcher;
    return true;
}
#endif

namespace unixlib {

std::optional<std::string> init() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    if (!ntdll || !kernel32 || !init_dispatcher(ntdll)) {
        return "This doesn't look like Wine, ntdll has no unix call dispatcher";
    }

    wine_get_dos_file_name_ptr = reinterpret_cast<wine_get_dos_file_name_t>(
        GetProcAddress(kernel32, "wine_get_dos_file_name"));
    wine_get_unix_file_name_ptr = reinterpret_cast<wine_get_unix_file_name_t>(
        GetProcAddress(kernel32, "wine_get_unix_file_name"));
    auto nt_query_virtual_memory = reinterpret_cast<NtQueryVirtualMemory_t>(
        GetProcAddress(ntdll, "NtQueryVirtualMemory"));
    if (!wine_get_dos_file_name_ptr || !wine_get_unix_file_name_ptr ||
        !nt_query_virtual_memory) {
        return "Could not find Wine's ntdll and kernel32 exports";
    }

    // The unixlib is installed next to this executable as
    // `<name>-unixlib.so`
    std::wstring exe_path(MAX_PATH, L'\0');
    DWORD length;
    while ((length = GetModuleFileNameW(nullptr, exe_path.data(),
                                        static_cast<DWORD>(exe_path.size()))) ==
           exe_path.size()) {
        exe_path.resize(exe_path.size() * 2);
    }
    exe_path.resize(length);
    if (exe_path.size() >= 4 &&
        _wcsicmp(exe_path.c_str() + exe_path.size() - 4, L".exe") == 0) {
        exe_path.resize(exe_path.size() - 4);
    }

    // `load_unixlib_by_name()` in `wine/dlls/ntdll/unix/loader.c` treats names
    // containing a path separator as an NT path
    std::wstring nt_path = L"\\??\\" + exe_path + L"-unixlib.so";
    UNICODE_STRING name;
    name.Buffer = nt_path.data();
    name.Length = name.MaximumLength =
        static_cast<USHORT>(nt_path.size() * sizeof(wchar_t));

    // `Result[0]` is the `dlopen()` handle, and `Result[1]` the handle passed
    // to the dispatcher
    uint64_t result[2]{};
    const NTSTATUS status = nt_query_virtual_memory(
        GetCurrentProcess(), &name, MemoryWineLoadUnixLibByName, result,
        sizeof(result), nullptr);
    if (status != 0) {
        char message[64];
        snprintf(message, sizeof(message), "status 0x%08lx",
                 static_cast<unsigned long>(status));

        const std::string path(nt_path.begin(), nt_path.end());
        return "Could not load the unixlib at '" + path + "' (" + message +
               "). This needs Wine 11.2 or newer.";
    }

    unixlib_handle = result[1];
    return std::nullopt;
}

void call(Call code, void* args) {
    vstbridge_unix_call(unixlib_handle, code, args);
}

wchar_t* wine_get_dos_file_name(const char* unix_path) {
    return wine_get_dos_file_name_ptr(unix_path);
}

char* wine_get_unix_file_name(const wchar_t* dos_path) {
    return wine_get_unix_file_name_ptr(dos_path);
}

namespace {

std::string wide_to_multibyte(UINT code_page, const wchar_t* string) {
    const int size = WideCharToMultiByte(code_page, 0, string, -1, nullptr, 0,
                                         nullptr, nullptr);
    if (size <= 0) {
        return "";
    }

    std::string result(size, '\0');
    WideCharToMultiByte(code_page, 0, string, -1, result.data(), size, nullptr,
                        nullptr);
    // Drop the null terminator included in `size`
    result.resize(size - 1);

    return result;
}

}  // namespace

std::vector<std::string> command_line_arguments() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
        return {};
    }

    std::vector<std::string> arguments;
    for (int i = 0; i < argc; i++) {
        arguments.push_back(wide_to_multibyte(CP_UTF8, argv[i]));
    }
    LocalFree(argv);

    return arguments;
}

std::string unix_path_to_dos_path(const std::string& unix_path) {
    WCHAR* dos_path = wine_get_dos_file_name(unix_path.c_str());
    if (!dos_path) {
        return unix_path;
    }

    std::string result = wide_to_multibyte(CP_ACP, dos_path);
    HeapFree(GetProcessHeap(), 0, dos_path);

    return result;
}

std::string ansi_to_utf8(const std::string& string) {
    const int size =
        MultiByteToWideChar(CP_ACP, 0, string.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return string;
    }

    std::wstring wide(size, L'\0');
    MultiByteToWideChar(CP_ACP, 0, string.c_str(), -1, wide.data(), size);

    return wide_to_multibyte(CP_UTF8, wide.c_str());
}

}  // namespace unixlib
