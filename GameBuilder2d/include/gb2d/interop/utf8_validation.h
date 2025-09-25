#pragma once
// utf8_validation.h (T026)
// Shared UTF-8 validation helper for interop layer (managed -> native).
// Current scope: simple validation used by window title & (future) other text inputs.
// Validation rules:
//  - Input must be non-null and non-empty
//  - Byte length (excluding terminator) must be <= maxBytes
//  - Rejects malformed leading / continuation bytes, overlong encodings,
//    surrogate range (U+D800..U+DFFF) and code points > U+10FFFF.
// Returns true on success and writes outLen (# of bytes before terminator).

#include <cstddef>

namespace gb2d::interop::utf8 {

inline bool validate(const char* s, std::size_t maxBytes, std::size_t& outLen) {
    outLen = 0;
    if (!s) return false;
    const unsigned char* u = reinterpret_cast<const unsigned char*>(s);
    while (*u) {
        if (outLen >= maxBytes) return false; // length guard (excludes terminator)
        unsigned char c = *u;
        std::size_t seqLen = 0;
        if (c <= 0x7F) { seqLen = 1; }
        else if ((c & 0xE0) == 0xC0) { if (c < 0xC2) return false; seqLen = 2; }
        else if ((c & 0xF0) == 0xE0) { seqLen = 3; }
        else if ((c & 0xF8) == 0xF0) { if (c > 0xF4) return false; seqLen = 4; }
        else return false; // invalid leading byte
        for (std::size_t i = 1; i < seqLen; ++i) {
            unsigned char cc = u[i];
            if ((cc & 0xC0) != 0x80) return false; // continuation byte invalid
        }
        if (seqLen == 3) {
            unsigned char c1 = u[0]; unsigned char c2 = u[1];
            if (c1 == 0xE0 && (c2 & 0xE0) == 0x80) return false; // overlong for < U+080
            if (c1 == 0xED && (c2 & 0xE0) == 0xA0) return false; // surrogate range
        } else if (seqLen == 4) {
            unsigned char c1 = u[0]; unsigned char c2 = u[1];
            if (c1 == 0xF0 && (c2 & 0xF0) == 0x80) return false; // overlong for < U+10000
            if (c1 == 0xF4 && c2 > 0x8F) return false; // > U+10FFFF
        }
        u += seqLen; outLen += seqLen;
    }
    return outLen > 0; // non-empty only
}

} // namespace gb2d::interop::utf8
