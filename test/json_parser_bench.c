#include "json_parser.c"
#include "platform.h"
#include "string_builder.h"

internalfn void
StringBuilderAppendHumanReadableDuration(struct string_builder *sb, struct duration duration)
{
  struct unit {
    u64 limit;
    struct string suffix;
  } units[] = {
      {.limit = 1000000000ul * 60 * 60, .suffix = StringFromLiteral("hr")},
      {.limit = 1000000000ul * 60, .suffix = StringFromLiteral("min")},
      {.limit = 1000000000ul, .suffix = StringFromLiteral("s")},
      {.limit = 1000000ul, .suffix = StringFromLiteral("ms")},
      {.limit = 1000ul, .suffix = StringFromLiteral("μs")},
      {.limit = 1ul, .suffix = StringFromLiteral("ns")},
  };

  u64 durationInNanoseconds = duration.ns;
  for (u32 unitIndex = 0; unitIndex < ARRAY_COUNT(units); unitIndex++) {
    struct unit *unit = units + unitIndex;
    if (durationInNanoseconds < unit->limit)
      continue;

    u64 value = durationInNanoseconds / unit->limit;
    durationInNanoseconds -= value * unit->limit;

    StringBuilderAppendU64(sb, value);
    StringBuilderAppendString(sb, &unit->suffix);
  }
}

internalfn inline void
StringBuilderAppendHumanReadableBytes(string_builder *sb, u64 bytes)
{
  struct order {
    u64 magnitude;
    struct string unit;
  } orders[] = {
      // order is important, bigger one first
      {.magnitude = (1UL << 50), .unit = StringFromLiteral("PiB")},
      {.magnitude = (1UL << 40), .unit = StringFromLiteral("TiB")},
      {.magnitude = (1UL << 30), .unit = StringFromLiteral("GiB")},
      {.magnitude = (1UL << 20), .unit = StringFromLiteral("MiB")},
      {.magnitude = (1UL << 10), .unit = StringFromLiteral("KiB")},
      {.magnitude = 1, .unit = StringFromLiteral("B")},
  };

  if (bytes == 0) {
    StringBuilderAppendStringLiteral(sb, "0");
    return;
  }

  for (u32 orderIndex = 0; orderIndex < ARRAY_COUNT(orders); orderIndex++) {
    struct order *order = orders + orderIndex;
    if (bytes >= order->magnitude) {
      u64 value = bytes / order->magnitude;
      bytes -= value * order->magnitude;

      StringBuilderAppendU64(sb, value);
      StringBuilderAppendString(sb, &order->unit);
      if (bytes != 0)
        StringBuilderAppendStringLiteral(sb, " ");
    }
  }
}

int
main(int argc, char *argv[])
{
  // setup
  enum {
    KILOBYTES = (1 << 10),
    MEGABYTES = (1 << 20),
  };

  u8 stackBuffer[2 * MEGABYTES];
  memory_arena stackMemory = {
      .block = stackBuffer,
      .total = ARRAY_COUNT(stackBuffer),
  };

  string_builder *sb = MakeStringBuilder(&stackMemory, 1024, 32);

  memory_arena heapMemory = {
      .total = 4 * MEGABYTES,
  };
  heapMemory.block = PlatformAllocate(heapMemory.total);
  if (!heapMemory.block) {
    StringBuilderAppendStringLiteral(sb, "Could not allocate ");
    StringBuilderAppendHumanReadableBytes(sb, heapMemory.total);
    StringBuilderAppendStringLiteral(sb, " memory");
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  string path = StringNull();
  for (u32 argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
    struct string argument = StringFromZeroTerminated((u8 *)argv[argumentIndex], 1024);
    if (!IsStringStartsWith(&argument, &StringFromLiteral("--"))) {
      path = argument;
    }
  }
  if (IsStringNullOrEmpty(&path)) {
    StringBuilderAppendStringLiteral(sb, "Needs json file as argument");
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
    return 1;
  }

  string json;
  string *fileBuffer = MakeString(&heapMemory, 2 * MEGABYTES);

  {
    u64 startedAt = NowInNanoseconds();
    enum platform_error error = PlatformReadFile(fileBuffer, &path, &json);
    if (error != IO_ERROR_NONE) {
      StringBuilderAppendStringLiteral(sb, "Could not read file.");
      StringBuilderAppendStringLiteral(sb, "\n  path: ");
      StringBuilderAppendString(sb, &path);
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }
    u64 elapsed = NowInNanoseconds() - startedAt;

    StringBuilderAppendStringLiteral(sb, "Loading json took ");
    StringBuilderAppendHumanReadableDuration(sb, DurationInNanoseconds(elapsed));
    StringBuilderAppendStringLiteral(sb, " ( ");
    StringBuilderAppendU64(sb, elapsed);
    StringBuilderAppendStringLiteral(sb, "ns )");
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
  }

  struct json_parser *parser = MakeJsonParser(&heapMemory, 50000);
  {
    u64 startedAt = NowInNanoseconds();
    b8 jsonParsed = JsonParse(parser, &json);
    if (!jsonParsed) {
      StringBuilderAppendStringLiteral(sb, "Could not parse json");
      StringBuilderAppendStringLiteral(sb, "\n");
      struct string message = StringBuilderFlush(sb);
      PrintString(&message);
      return 1;
    }
    u64 elapsed = NowInNanoseconds() - startedAt;

    StringBuilderAppendStringLiteral(sb, "Parsing json took ");
    StringBuilderAppendHumanReadableDuration(sb, DurationInNanoseconds(elapsed));
    StringBuilderAppendStringLiteral(sb, " ( ");
    StringBuilderAppendU64(sb, elapsed);
    StringBuilderAppendStringLiteral(sb, "ns )");
    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
  }

  for (u32 jsonTokenIndex = 0; 0 && jsonTokenIndex < parser->tokenCount; jsonTokenIndex++) {
    struct json_token *jsonToken = parser->tokens + jsonTokenIndex;
    struct string content = JsonTokenExtractString(jsonToken, &json);

    struct string type = StringFromLiteral("unknown");
    switch (jsonToken->type) {
    case JSON_TOKEN_NONE:
      type = StringFromLiteral("none");
      break;
    case JSON_TOKEN_NULL:
      type = StringFromLiteral("null");
      break;
    case JSON_TOKEN_OBJECT:
      type = StringFromLiteral("object");
      break;
    case JSON_TOKEN_ARRAY:
      type = StringFromLiteral("array");
      break;
    case JSON_TOKEN_STRING:
      type = StringFromLiteral("string");
      break;
    case JSON_TOKEN_BOOLEAN_FALSE:
    case JSON_TOKEN_BOOLEAN_TRUE:
      type = StringFromLiteral("boolean");
      break;
    case JSON_TOKEN_NUMBER:
      type = StringFromLiteral("number");
      break;
    }

    StringBuilderAppendStringLiteral(sb, "Token #");
    StringBuilderAppendU32(sb, jsonTokenIndex);

    StringBuilderAppendStringLiteral(sb, " ");
    StringBuilderAppendString(sb, &type);

    if (jsonToken->type == JSON_TOKEN_STRING || jsonToken->type == JSON_TOKEN_BOOLEAN_FALSE ||
        jsonToken->type == JSON_TOKEN_BOOLEAN_TRUE || jsonToken->type == JSON_TOKEN_NUMBER) {
      StringBuilderAppendStringLiteral(sb, " '");
      StringBuilderAppendString(sb, &content);
      StringBuilderAppendStringLiteral(sb, "'");
    }

    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
  }

  {
    struct json_cursor cursor = JsonCursor(&json, parser);
    if (!JsonCursorIsObject(&cursor))
      return 1; // error invalid json
    if (!JsonCursorNext(&cursor))
      return 1; // error invalid json

    while (!JsonCursorIsAtEnd(&cursor)) {
      struct string key = JsonCursorExtractString(&cursor);
      if (IsStringEqual(&key, &StringFromLiteral("search_metadata")))
        break;
      JsonCursorNextKey(&cursor);
    }

    if (!JsonCursorNext(&cursor))
      return 1; // error invalid json
    if (!JsonCursorIsObject(&cursor))
      return 1; // error invalid json
    if (!JsonCursorNext(&cursor))
      return 1; // error invalid json

    while (!JsonCursorIsAtEnd(&cursor)) {
      struct string key = JsonCursorExtractString(&cursor);
      if (IsStringEqual(&key, &StringFromLiteral("count")))
        break;
      JsonCursorNextKey(&cursor);
    }

    if (!JsonCursorNext(&cursor))
      return 1; // error invalid json
    if (!JsonCursorIsNumber(&cursor))
      return 1; // error invalid json

    struct string count = JsonCursorExtractString(&cursor);
    StringBuilderAppendStringLiteral(sb, ".search_metadata.count is ");
    StringBuilderAppendString(sb, &count);

    StringBuilderAppendStringLiteral(sb, "\n");
    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
  }

#if 1 && IS_BUILD_DEBUG
  {
    StringBuilderAppendStringLiteral(sb, "Stack Memory:");
    StringBuilderAppendStringLiteral(sb, "\n  total:  ");
    StringBuilderAppendHumanReadableBytes(sb, stackMemory.total);
    StringBuilderAppendStringLiteral(sb, "\n  used:   ");
    StringBuilderAppendHumanReadableBytes(sb, stackMemory.used);
    StringBuilderAppendStringLiteral(sb, "\n  wasted: ");
    StringBuilderAppendHumanReadableBytes(sb, stackMemory.total - stackMemory.used);
    StringBuilderAppendStringLiteral(sb, "\n");

    StringBuilderAppendStringLiteral(sb, "Heap Memory:");
    StringBuilderAppendStringLiteral(sb, "\n  total:  ");
    StringBuilderAppendHumanReadableBytes(sb, heapMemory.total);
    StringBuilderAppendStringLiteral(sb, "\n  used:   ");
    StringBuilderAppendHumanReadableBytes(sb, heapMemory.used);
    StringBuilderAppendStringLiteral(sb, "\n  wasted: ");
    StringBuilderAppendHumanReadableBytes(sb, heapMemory.total - heapMemory.used);
    StringBuilderAppendStringLiteral(sb, "\n");

    struct string message = StringBuilderFlush(sb);
    PrintString(&message);
  }
#endif

  return 0;
}
