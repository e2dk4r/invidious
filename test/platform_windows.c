#include "assert.h"

#include <ntstatus.h>
#include <windows.h>

internalfn void
PrintString(struct string *string)
{
  HANDLE outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  (void)WriteFile(outputHandle, string->value, (u32)string->length, 0, 0);
}

internalfn u64
NowInNanoseconds(void)
{
  static LARGE_INTEGER frequency = {0};

  if (frequency.QuadPart == 0)
    if (!QueryPerformanceFrequency(&frequency))
      runtime_assert(0 && "clock is unstable");

  LARGE_INTEGER tick;
  if (!QueryPerformanceCounter(&tick))
    runtime_assert(0 && "clock is unstable");

  return (u64)(tick.QuadPart * (1000000000 /*1e9*/ / frequency.QuadPart));
}

internalfn void
StringBuilderAppendPlatformError(struct string_builder *sb)
{
  DWORD errnum = GetLastError();

  char buffer[2048];
  struct string message = StringFromLiteral("None");
  if (errnum != 0) {
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 0, errnum, 0, buffer,
                   ARRAY_COUNT(buffer), 0);
    message = StringFromZeroTerminated((u8 *)buffer, ARRAY_COUNT(buffer));
  }

  StringBuilderAppendString(sb, &message);

  StringBuilderAppendStringLiteral(sb, " (Errno ");
  StringBuilderAppendS32(sb, errno);
  StringBuilderAppendStringLiteral(sb, ")");
}

internalfn void *
PlatformAllocate(u64 size)
{
  return VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

internalfn enum platform_error
PlatformGetRandom(struct string *buffer)
{
  NTSTATUS status = BCryptGenRandom(0, buffer->value, (u32)buffer->length, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (status != STATUS_SUCCESS)
    return IO_ERROR_PLATFORM;

  return IO_ERROR_NONE;
}

internalfn b8
PlatformIsFileExists(struct string *path, enum platform_error *error)
{
  debug_assert(path->value[path->length] == 0 && "must be zero terminated");
  DWORD attributes = GetFileAttributesA((char *)path->value);
  return (attributes != INVALID_FILE_ATTRIBUTES) && !(attributes & FILE_ATTRIBUTE_NORMAL);
}

internalfn enum platform_error
PlatformReadFile(struct string *buffer, struct string *path, struct string *content)
{
  debug_assert(path->value[path->length] == 0 && "must be zero-terminated string");
  enum platform_error error = IO_ERROR_NONE;

  HANDLE file =
      CreateFileA((char *)path->value, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (file == INVALID_HANDLE_VALUE)
    return IO_ERROR_FILE_NOT_FOUND;

  struct string_cursor bufferCursor = StringCursorFromString(buffer);
  while (1) {
    struct string remainingBuffer = StringCursorExtractRemaining(&bufferCursor);
    if (IsStringCursorAtEnd(&bufferCursor)) {
      error = IO_ERROR_BUFFER_OUT_OF_MEMORY;
      goto failure;
    }

    DWORD readBytes;
    BOOL isRead = ReadFile(file, remainingBuffer.value, (u32)remainingBuffer.length, &readBytes, 0);
    if (!isRead) {
      error = IO_ERROR_PLATFORM;
      goto failure;
    } else if (readBytes == 0) {
      // end of file
      break;
    }

    bufferCursor.position += (u64)readBytes;
  }

  CloseHandle(file);

  *content = StringCursorExtractConsumed(&bufferCursor);
  return error;

failure:
  CloseHandle(file);
  return error;
}
