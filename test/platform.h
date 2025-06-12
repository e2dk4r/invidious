#pragma once
#include "string_builder.h"

enum platform_error {
  IO_ERROR_NONE,
  IO_ERROR_FILE_NOT_FOUND,
  IO_ERROR_BUFFER_OUT_OF_MEMORY,
  IO_ERROR_BUFFER_PARTIALLY_FILLED,
  IO_ERROR_PLATFORM,
};

internalfn void
PrintString(struct string *string);

internalfn u64
NowInNanoseconds(void);

internalfn void
StringBuilderAppendPlatformError(struct string_builder *sb);

internalfn void *
PlatformAllocate(u64 size);

internalfn enum platform_error
PlatformGetRandom(struct string *buffer);

internalfn b8
PlatformIsFileExists(struct string *path, enum platform_error *error);

internalfn enum platform_error
PlatformReadFile(struct string *buffer, struct string *path, struct string *content);

#if IS_PLATFORM_LINUX
#include "platform_linux.c"
#elif IS_PLATFORM_WINDOWS
#include "platform_windows.c"
#endif
