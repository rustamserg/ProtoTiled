#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static constexpr size_t MAX_PATH_LENGTH = 1024;

// heap copy of an optional string, nullptr stays nullptr
[[nodiscard]] static inline char* CopyString(const char* text)
{
    return text ? strdup(text) : nullptr;
}

// null safe string comparison, nullptr equals only nullptr
[[nodiscard]] static inline bool StringEquals(const char* a, const char* b)
{
    return a == b || (a && b && strcmp(a, b) == 0);
}

// resolves path that is relative to the file baseFile, absolute paths are copied as is
[[nodiscard]] static inline bool ResolveRelativePath(char* out, size_t size, const char* baseFile, const char* relative)
{
    const bool absolute = relative[0] == '/' || relative[0] == '\\' || (relative[0] != '\0' && relative[1] == ':');

    int directoryLength = 0;
    if (!absolute)
    {
        for (int i = 0; baseFile[i] != '\0'; ++i)
        {
            if (baseFile[i] == '/' || baseFile[i] == '\\')
            {
                directoryLength = i + 1;
            }
        }
    }

    const int written = snprintf(out, size, "%.*s%s", directoryLength, baseFile, relative);
    return written >= 0 && (size_t)written < size;
}
