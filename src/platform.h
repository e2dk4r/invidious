#pragma once

#include "memory.h"
#include "type.h"

internalfn struct memory_arena
PlatformMemoryAllocate(u64 size);

internalfn u64
PlatformFileSize(struct string *path);

internalfn struct string
PlatformReadFile(struct string *buffer, struct string *path);

internalfn u64
PlatformWriteFile(struct string *buffer, struct string *path);

#include "string_builder.h"
internalfn void
StringBuilderAppendPlatformError(struct string_builder *sb);

#if IS_PLATFORM_LINUX
#include "platform_linux.c"
#endif
