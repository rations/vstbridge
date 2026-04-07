#pragma once

#include "funknown.h"

namespace Steinberg {

class IBStream : public FUnknown {
public:
    virtual tresult PLUGIN_API read(void* buffer, int32 numBytes, int32* numBytesRead = nullptr) = 0;
    virtual tresult PLUGIN_API write(void* buffer, int32 numBytes, int32* numBytesWritten = nullptr) = 0;
    virtual tresult PLUGIN_API seek(int64 pos, int32 mode, int64* result = nullptr) = 0;
    virtual tresult PLUGIN_API tell(int64* pos) = 0;

    static const TUID iid;
};

const TUID IBStream::iid = {0xA2, 0x6E, 0xBF, 0xC3, 0x52, 0x47, 0x99, 0x30, 0x90, 0xF9, 0x6B, 0x9B, 0x9B, 0x3E, 0xE3, 0x1E};

} // namespace Steinberg