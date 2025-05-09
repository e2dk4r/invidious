#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "assert.h"

internalfn struct memory_arena
PlatformMemoryAllocate(u64 size)
{
  struct memory_arena arena;

  arena = (struct memory_arena){.used = 0, .total = 0};
  arena.block = mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (arena.block == MAP_FAILED) {
    arena.block = 0;
    return arena;
  }
  arena.total = size;

  return arena;
}

internalfn u64
PlatformFileSize(struct string *path)
{
  struct stat stat;

  debug_assert(path->value[path->length] == 0 && "must be zero terminated");
  if (lstat((char *)path->value, &stat) == -1)
    return 0;

  if (!S_ISREG(stat.st_mode))
    return 0;

  if (stat.st_size < 0)
    return 0;

  return (u64)stat.st_size;
}

internalfn struct string
PlatformReadFile(struct string *buffer, struct string *path)
{
  int fd;
  ssize_t bytesRead;
  struct string content;
  struct string_cursor cursor;

  debug_assert(path->value[path->length] == 0 && "must be zero terminated");
  debug_assert(!IsStringNullOrEmpty(buffer));

  content = StringNull();
  fd = open((char *)path->value, O_RDONLY);
  if (fd < 0)
    goto exit;

  cursor = StringCursorFromString(buffer);
  while (1) {
    struct string remaining = StringCursorExtractRemaining(&cursor);
    if (remaining.length == 0)
      break;

    bytesRead = read(fd, remaining.value, remaining.length);
    if (bytesRead == 0)
      break; // end of file
    else if (bytesRead < 0)
      goto close; // error

    cursor.position += (u64)bytesRead;
  }

  content = StringSlice(buffer, 0, cursor.position);

close:
  close(fd);
exit:
  return content;
}

internalfn u64
PlatformWriteFile(struct string *buffer, struct string *path)
{
  int fd;
  ssize_t bytesWritten;
  struct string content;
  struct string_cursor cursor;

  debug_assert(path->value[path->length] == 0 && "must be zero terminated");
  debug_assert(!IsStringNullOrEmpty(buffer));

  cursor = StringCursorFromString(buffer);

  fd = open((char *)path->value, O_CREAT | O_WRONLY, 0644);
  if (fd < 0)
    goto exit;

  while (1) {
    struct string remaining = StringCursorExtractRemaining(&cursor);
    if (remaining.length == 0)
      break;

    bytesWritten = write(fd, remaining.value, remaining.length);
    if (bytesWritten < 0)
      goto close; // error

    cursor.position += (u64)bytesWritten;
  }

close:
  close(fd);
exit:
  return cursor.position;
}

#include <errno.h>
#include <string.h>
internalfn void
StringBuilderAppendPlatformError(struct string_builder *sb)
{
  struct string message = StringFromZeroTerminated((u8 *)strerror(errno), 4096);
  StringBuilderAppendString(sb, &message);

  StringBuilderAppendStringLiteral(sb, " (Errno ");
  StringBuilderAppendS64(sb, errno);
  StringBuilderAppendStringLiteral(sb, ")");
}
