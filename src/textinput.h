#ifndef TEXTINPUT_H
#define TEXTINPUT_H

#include <SDL3/SDL_stdinc.h>
#include <algorithm>
#include <cstring>
#include <string>

// SDL text events contain UTF-8. Erase one codepoint, not one byte.
inline size_t Utf8BackspaceLength(const char* text, size_t length) {
    const char* end = text + length;
    SDL_StepBackUTF8(text, &end);
    return static_cast<size_t>(end - text);
}

inline void BackspaceUtf8(char* text) {
    text[Utf8BackspaceLength(text, std::strlen(text))] = '\0';
}

inline void BackspaceUtf8(std::string& text) {
    text.resize(Utf8BackspaceLength(text.c_str(), text.size()));
}

// Keep the existing byte limits required by the protocol, but never append
// half a codepoint. Android IMEs may deliver Backspace inside a text event.
template<size_t Capacity>
inline void AppendUtf8Input(char (&text)[Capacity], const char* input,
                           size_t maxBytes = Capacity - 1) {
    maxBytes = std::min(maxBytes, Capacity - 1);
    size_t length = std::strlen(text);
    while (*input) {
        const char* start = input;
        const Uint32 codepoint = SDL_StepUTF8(&input, nullptr);
        if (codepoint == '\b') {
            length = Utf8BackspaceLength(text, length);
        } else {
            const size_t bytes = static_cast<size_t>(input - start);
            if (length + bytes <= maxBytes) {
                std::memcpy(text + length, start, bytes);
                length += bytes;
            }
        }
        text[length] = '\0';
    }
}

#endif
