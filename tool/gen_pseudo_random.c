#include "print.h"
#include "string_builder.h"
#include "string_cursor.h"

// TODO: refactor parsing options to be more easy

struct options {
  u32 randomNumberCount;
  string templatePath;
};

internalfn void
OptionsInit(struct options *options)
{
  options->randomNumberCount = 4096;
  options->templatePath = StringFromLiteral("");
}

struct context {
  struct options options;
  string_builder *sb;
};

#if IS_PLATFORM_LINUX

#include <errno.h>
#include <sys/random.h>

internalfn b8
GetRandom(struct context *context, struct string *buffer)
{
  struct string_builder *sb = context->sb;
  s64 result = getrandom(buffer->value, buffer->length, 0);
  if (result < 0) {
    StringBuilderAppendStringLiteral(sb, "Error: getrandom() failed with ");
    StringBuilderAppendS64(sb, errno);
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 0;
  }

  if ((u64)result != buffer->length) {
    StringBuilderAppendStringLiteral(sb, "Error: getrandom() insufficent entropy\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 0;
  }

  return 1;
}

#include <sys/stat.h>
internalfn b8
IsFileExists(struct string *path)
{
  debug_assert(path->value[path->length] == 0 && "must be zero-terminated string");

  struct stat sb;
  if (lstat((char *)path->value, &sb))
    return 0;

  // if it is not regular file. man inode.7
  if (!S_ISREG(sb.st_mode))
    return 0;

  return 1;
}

#include <fcntl.h>
internalfn struct string
ReadFile(struct string *buffer, struct string *path)
{
  debug_assert(path->value[path->length] == 0 && "must be zero-terminated string");
  struct string result = {};

  int fd = open((char *)path->value, O_RDONLY);
  if (fd < 0) {
    // error
    return result;
  }

  struct string_cursor bufferCursor = StringCursorFromString(buffer);
  while (1) {
    struct string remainingBuffer = StringCursorExtractRemaining(&bufferCursor);

    s64 readBytes = read(fd, remainingBuffer.value, remainingBuffer.length);
    if (readBytes == -1) {
      // error
      return result;
    } else if (readBytes == 0) {
      // end of file
      break;
    }

    bufferCursor.position += (u64)readBytes;
  }

  close(fd);

  result = StringCursorExtractConsumed(&bufferCursor);

  return result;
}

#elif IS_PLATFORM_WINDOWS

// TODO: test it on windows
#include <bcrypt.h>

internalfn b8
GetRandom(string *buffer)
{
  NTSTATUS status = BCryptGenRandom(0, buffer->value, buffer->length, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
  if (status != STATUS_SUCCESS) {
    StringBuilderAppendStringLiteral(sb, "Error: BCryptGenRandom() failed with ");
    StringBuilderAppendU64(sb, status);
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 0;
  }

  return 1;
}

#endif

int
main(int argc, char *argv[])
{
  // setup
  struct context context = {};
  struct options *options = &context.options;
  OptionsInit(options);

  u32 KILOBYTES = 1 << 10;
  u32 MEGABYTES = 1 << 20;
  u8 stackBuffer[1 * MEGABYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 128 * KILOBYTES, 32);
  context.sb = sb;

  // parse options
  for (u32 argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
    string option = StringFromZeroTerminated((u8 *)argv[argumentIndex], 1024);

    if (IsStringEqual(&option, &StringFromLiteral("--template"))) {
      argumentIndex++;
      if (argumentIndex == argc) {
        PrintString(&StringFromLiteral("Template is required to take a file\n"));
        return -1;
      }

      string value = StringFromZeroTerminated((u8 *)argv[argumentIndex], 1024);
      if (!IsFileExists(&value)) {
        StringBuilderAppendStringLiteral(sb, "Template at '");
        StringBuilderAppendString(sb, &value);
        StringBuilderAppendStringLiteral(sb, "' is not found\n");
        string message = StringBuilderFlush(sb);
        PrintString(&message);
        return -1;
      }

      options->templatePath = value;
    } else if (IsStringEqual(&option, &StringFromLiteral("--count"))) {
      argumentIndex++;
      if (argumentIndex == argc) {
        PrintString(&StringFromLiteral("Count is required to take positive value\n"));
        return -1;
      }
      string value = StringFromZeroTerminated((u8 *)argv[argumentIndex], 1024);

      b8 isHex = 0;
      if (IsStringStartsWith(&value, &StringFromLiteral("0x"))) {
        value = StringSlice(&value, 2, value.length);
        isHex = 1;
      }

      u64 randomNumberCount;
      u32 randomNumberMin = 1;
      u32 randomNumberMax = 200000;

      b8 parseOK = ParseU64(&value, &randomNumberCount);
      if (isHex)
        parseOK = ParseHex(&value, &randomNumberCount);

      if (!parseOK || randomNumberCount < randomNumberMin || randomNumberCount > randomNumberMax) {
        StringBuilderAppendStringLiteral(sb, "Expected positive value between [");
        StringBuilderAppendU32(sb, randomNumberMin);
        StringBuilderAppendStringLiteral(sb, ", ");
        StringBuilderAppendU32(sb, randomNumberMax);
        StringBuilderAppendStringLiteral(sb, "]\n");
        string message = StringBuilderFlush(sb);
        PrintString(&message);
        return -1;
      }

      options->randomNumberCount = (u32)randomNumberCount;
    } else if (IsStringEqual(&option, &StringFromLiteral("-h")) ||
               IsStringEqual(&option, &StringFromLiteral("--help"))) {
      StringBuilderAppendStringLiteral(sb, "NAME");
      StringBuilderAppendStringLiteral(sb, "\n  gen_pseudo_random - Generate pseudo random numbers with template");
      StringBuilderAppendStringLiteral(sb, "\n\nSYNOPSIS:");
      StringBuilderAppendStringLiteral(sb, "\n  gen_pseudo_random --template path [OPTIONS]");
      StringBuilderAppendStringLiteral(sb, "\n\nTEMPLATE:");
      StringBuilderAppendStringLiteral(
          sb, "\n  In template file you can specify below variables with prefix and postfix $$");
      StringBuilderAppendStringLiteral(sb, "\n  (two dollar signs).");
      StringBuilderAppendStringLiteral(sb, "\n  ");
      StringBuilderAppendStringLiteral(sb, "\n  RANDOM_NUMBER_TABLE");
      StringBuilderAppendStringLiteral(sb, "\n    Comma seperated list of u32 in hex format. Range is [0, 4294967295]");
      StringBuilderAppendStringLiteral(sb, "\n  RANDOM_NUMBER_COUNT");
      StringBuilderAppendStringLiteral(sb, "\n    Count of random numbers");
      StringBuilderAppendStringLiteral(sb, "\n  RANDOM_NUMBER_MIN");
      StringBuilderAppendStringLiteral(sb, "\n    Minimum (smallest) random number in table");
      StringBuilderAppendStringLiteral(sb, "\n  RANDOM_NUMBER_MAX");
      StringBuilderAppendStringLiteral(sb, "\n    Maximum (biggest) random number in table");
      StringBuilderAppendStringLiteral(sb, "\n\nOPTIONS:");
      StringBuilderAppendStringLiteral(sb, "\n  --template path");
      StringBuilderAppendStringLiteral(sb, "\n    Location of template file");
      StringBuilderAppendStringLiteral(sb, "\n    This options is required");
      StringBuilderAppendStringLiteral(sb, "\n  --count count");
      StringBuilderAppendStringLiteral(sb, "\n    How many random numbers must be generated");
      StringBuilderAppendStringLiteral(sb, "\n    Range is [1, 200000]");
      StringBuilderAppendStringLiteral(sb, "\n  -h, --help");
      StringBuilderAppendStringLiteral(sb, "\n    Show this help message");
      StringBuilderAppendStringLiteral(sb, "\n");
      string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 0;
    } else {
      StringBuilderAppendStringLiteral(sb, "Option '");
      StringBuilderAppendString(sb, &option);
      StringBuilderAppendStringLiteral(sb, "' is not understand");
      StringBuilderAppendStringLiteral(sb, "\nSee --help for more information");
      StringBuilderAppendStringLiteral(sb, "\n");
      string message = StringBuilderFlush(sb);
      PrintString(&message);
      return -1;
    }
  }

  if (IsStringNull(&options->templatePath) || IsStringEmpty(&options->templatePath)) {
    StringBuilderAppendStringLiteral(sb, "--template option is required");
    StringBuilderAppendStringLiteral(sb, "\nSee --help for more information");
    StringBuilderAppendStringLiteral(sb, "\n");
    string message = StringBuilderFlush(sb);
    PrintString(&message);
    return -1;
  }

  // generate
  u32 bytesPerRandomNumber = sizeof(u32);
  u32 *randomNumbers = MemoryArenaPush(&stackMemory, options->randomNumberCount * bytesPerRandomNumber);
  u32 randomNumberMinIndex = 0;
  u32 randomNumberMaxIndex = 0;
  {
    memory_temp tempMemory = MemoryTempBegin(&stackMemory);
    string *randomBuffer = MakeString(tempMemory.arena, options->randomNumberCount * bytesPerRandomNumber);
    string_cursor randomCursor = StringCursorFromString(randomBuffer);
    randomCursor.position = StringCursorRemainingLength(&randomCursor);

    u32 min;
    u32 max;
    for (u32 randomNumberIndex = 0; randomNumberIndex < options->randomNumberCount; randomNumberIndex++) {
      if (IsStringCursorAtEnd(&randomCursor)) {
        if (!GetRandom(&context, randomBuffer))
          return -1;
        randomCursor = StringCursorFromString(randomBuffer);
      }

      string slice = StringCursorConsumeSubstring(&randomCursor, bytesPerRandomNumber);

      u32 randomNumber = 0;
      for (u32 sliceIndex = 0; sliceIndex < slice.length; sliceIndex++) {
        randomNumber *= 10;
        randomNumber += slice.value[sliceIndex];
      }

      if (randomNumberIndex == 0) {
        min = randomNumber;
        max = randomNumber;
      } else {
        if (randomNumber < min) {
          min = randomNumber;
          randomNumberMinIndex = randomNumberIndex;
        }

        if (randomNumber > max) {
          max = randomNumber;
          randomNumberMaxIndex = randomNumberIndex;
        }
      }

      *(randomNumbers + randomNumberIndex) = randomNumber;
    }

    MemoryTempEnd(&tempMemory);
  }

  // Read template
  string *templateBuffer = MakeString(&stackMemory, 256 * KILOBYTES);
  string template = ReadFile(templateBuffer, &options->templatePath);

  // TODO: Write output to a file

  // Replace variables with values
  string_cursor templateCursor = StringCursorFromString(&template);
  while (1) {
    string *variableMagicStart = &StringFromLiteral("$$");
    string *variableMagicEnd = variableMagicStart;

    string beforeVariable = StringCursorConsumeUntil(&templateCursor, variableMagicStart);
    if (beforeVariable.length != 0)
      PrintString(&beforeVariable);
    if (IsStringCursorAtEnd(&templateCursor))
      break;
    templateCursor.position += variableMagicStart->length;

    u64 variableStartPosition = templateCursor.position;
    string variable = StringCursorConsumeUntil(&templateCursor, variableMagicEnd);
    if (IsStringCursorAtEnd(&templateCursor))
      break;

    templateCursor.position += variableMagicEnd->length;

    if (IsStringEqual(&variable, &StringFromLiteral("RANDOM_NUMBER_TABLE"))) {
      for (u32 randomNumberIndex = 0; randomNumberIndex < options->randomNumberCount; randomNumberIndex++) {
        u32 randomNumber = *(randomNumbers + randomNumberIndex);

        StringBuilderAppendStringLiteral(sb, "0x");
        string randomNumberInHex = FormatHex(sb->stringBuffer, randomNumber);
        if (randomNumberInHex.length < 8) {
          for (u32 prefixIndex = 0; prefixIndex < 8 - randomNumberInHex.length; prefixIndex++) {
            StringBuilderAppendStringLiteral(sb, "0");
          }
        }
        StringBuilderAppendString(sb, &randomNumberInHex);

        if (randomNumberIndex + 1 != options->randomNumberCount)
          StringBuilderAppendStringLiteral(sb, ", ");
      }

      string message = StringBuilderFlush(sb);
      PrintString(&message);
    } else if (IsStringEqual(&variable, &StringFromLiteral("RANDOM_NUMBER_COUNT"))) {
      StringBuilderAppendU64(sb, options->randomNumberCount);
      string message = StringBuilderFlush(sb);
      PrintString(&message);
    } else if (IsStringEqual(&variable, &StringFromLiteral("RANDOM_NUMBER_MIN"))) {
      u32 randomNumberMin = *(randomNumbers + randomNumberMinIndex);
      StringBuilderAppendU64(sb, randomNumberMin);
      string message = StringBuilderFlush(sb);
      PrintString(&message);
    } else if (IsStringEqual(&variable, &StringFromLiteral("RANDOM_NUMBER_MAX"))) {
      u32 randomNumberMax = *(randomNumbers + randomNumberMaxIndex);
      StringBuilderAppendU64(sb, randomNumberMax);
      string message = StringBuilderFlush(sb);
      PrintString(&message);
    } else {
      StringBuilderAppendStringLiteral(sb, "Variable '");
      StringBuilderAppendString(sb, &variable);
      StringBuilderAppendStringLiteral(sb, "' at: ");
      StringBuilderAppendU64(sb, variableStartPosition);
      StringBuilderAppendStringLiteral(sb, " is NOT identified\n");
      string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }
  }
}
