/*
 * Inspired from https://github.com/zserge/jsmn by Serge Zaitsev
 *
 * Tokenizes json string.
 *
 * NOTE: Streaming concept was unsuccessful. Maybe my implementation was wrong?
 *       See git-blame for failed attempt.
 */
#include "assert.h"
#include "memory.h"
#include "string_cursor.h"
#include "type.h"

enum json_token_type {
  JSON_TOKEN_NONE,
  JSON_TOKEN_NULL,
  JSON_TOKEN_OBJECT,
  JSON_TOKEN_ARRAY,
  JSON_TOKEN_STRING,
  JSON_TOKEN_BOOLEAN_FALSE,
  JSON_TOKEN_BOOLEAN_TRUE,
  JSON_TOKEN_NUMBER,
};

enum json_parser_error {
  JSON_PARSER_ERROR_NONE,
  JSON_PARSER_ERROR_OUT_OF_TOKENS,
  JSON_PARSER_ERROR_NO_OPENING_BRACKET,
  JSON_PARSER_ERROR_PARTIAL,
  JSON_PARSER_ERROR_INVALID_BOOLEAN,
  JSON_PARSER_ERROR_INVALID_CHAR,
};

struct json_token {
  enum json_token_type type;
  u64 start; // start index of token in json string
  u64 end;   // end index of token in json string
};

struct json_parser {
  enum json_parser_error error;
  u32 tokenCount;
  u32 tokenMax;
  struct json_token *tokens;
};

internalfn void
JsonParserInit(struct json_parser *parser, struct json_token *tokens, u32 tokenCount)
{
  parser->tokenCount = 0;
  parser->tokenMax = tokenCount;
  parser->tokens = tokens;
}

internalfn struct json_parser
JsonParser(struct json_token *tokens, u32 tokenCount)
{
  struct json_parser parser;
  JsonParserInit(&parser, tokens, tokenCount);
  return parser;
}

internalfn struct json_parser *
MakeJsonParser(memory_arena *arena, u32 tokenCount)
{
  struct json_token *tokens = MemoryArenaPushUnaligned(arena, sizeof(*tokens) * tokenCount);
  struct json_parser *parser = MemoryArenaPushUnaligned(arena, sizeof(*parser));
  JsonParserInit(parser, tokens, tokenCount);
  return parser;
}

internalfn struct json_token *
JsonParserGetToken(struct json_parser *parser, u32 index)
{
  debug_assert(index < parser->tokenMax);
  if (index >= parser->tokenMax)
    return 0;

  return parser->tokens + index;
}

internalfn struct string
JsonTokenExtractString(struct json_token *token, struct string *json)
{
  debug_assert(token);
  debug_assert(token->end > token->start);
  u64 start = token->start;
  u64 length = token->end - start;
  return StringFromBuffer(json->value + start, length);
}

internalfn b8
JsonParse(struct json_parser *parser, struct string *json)
{
  u32 writtenTokenCount = 0;

  parser->error = JSON_PARSER_ERROR_NONE;

  struct string_cursor cursor = StringCursorFromString(json);
  while (!IsStringCursorAtEnd(&cursor)) {
    struct json_token *newToken = JsonParserGetToken(parser, writtenTokenCount);

    // JSON_TOKEN_OBJECT
    if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("{"))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      writtenTokenCount++;
      token->type = JSON_TOKEN_OBJECT;
      token->start = cursor.position;
      // invalid, must be set when closing bracket encountered
      token->end = 0;

      cursor.position += 1;
    }

    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("}"))) {
      struct json_token *startToken = 0;
      u32 startTokenIndex = writtenTokenCount - 1;
      while (1) {
        struct json_token *testToken = parser->tokens + startTokenIndex;
        if (testToken->type == JSON_TOKEN_OBJECT && testToken->end == 0) {
          startToken = testToken;
          break;
        }

        if (startTokenIndex == 0)
          break;

        startTokenIndex--;
      }
      if (!startToken) {
        // error object start token not found
        parser->error = JSON_PARSER_ERROR_NO_OPENING_BRACKET;
        return 0;
      }

      cursor.position += 1;
      startToken->end = cursor.position;
    }

    // JSON_TOKEN_ARRAY
    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("["))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }
      writtenTokenCount++;
      token->type = JSON_TOKEN_ARRAY;
      token->start = cursor.position;
      // invalid, must be set when closing bracket encountered
      token->end = 0;

      cursor.position += 1;
    }

    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("]"))) {
      struct json_token *startToken = 0;
      u32 startTokenIndex = writtenTokenCount - 1;
      while (1) {
        struct json_token *testToken = parser->tokens + startTokenIndex;
        if (testToken->type == JSON_TOKEN_ARRAY && testToken->end == 0) {
          startToken = testToken;
          break;
        }

        if (startTokenIndex == 0)
          break;

        startTokenIndex--;
      }
      if (!startToken) {
        // error object start token not found
        parser->error = JSON_PARSER_ERROR_NO_OPENING_BRACKET;
        return 0;
      }

      cursor.position += 1;
      startToken->end = cursor.position;
    }

    // whitespace
    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("\t")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("\r")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("\n")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral(" ")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral(":")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral(","))) {
      cursor.position += 1;
    }

    // JSON_TOKEN_STRING
    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("\""))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      writtenTokenCount++;
      cursor.position += 1;
      token->type = JSON_TOKEN_STRING;
      token->start = cursor.position;

      struct string_cursor tempCursor = cursor;
      struct string string = StringCursorConsumeUntil(&tempCursor, &StringFromLiteral("\""));
      // handle escaped double quotes
      while (IsStringEndsWith(&string, &StringFromLiteral("\\"))) {
        tempCursor.position += 1; // Advance after double quotes
        string.length += 1;
        struct string nextPart = StringCursorConsumeUntil(&tempCursor, &StringFromLiteral("\""));
        if (nextPart.length == 0)
          break;
        string.length += nextPart.length;
      }

      if (StringCursorRemainingLength(&cursor) == string.length) {
        // error string not closed
        parser->error = JSON_PARSER_ERROR_PARTIAL;
        return 0;
      }
      token->end = cursor.position + string.length;

      cursor.position += string.length + 1;
    }

    // JSON_TOKEN_NULL
    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("n"))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      writtenTokenCount++;
      token->type = JSON_TOKEN_NULL;
      token->start = cursor.position;

      struct string *expected = &StringFromLiteral("null");
      struct string got = StringCursorExtractSubstring(&cursor, expected->length);
      if (!IsStringEqual(&got, expected)) {
        parser->error = JSON_PARSER_ERROR_INVALID_CHAR;
        return 0;
      } else if (StringCursorRemainingLength(&cursor) == got.length) {
        // error null not finished
        parser->error = JSON_PARSER_ERROR_PARTIAL;
        return 0;
      }

      token->end = cursor.position + got.length;
      cursor.position += got.length + 1;
    }

    // JSON_TOKEN_BOOLEAN
    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("t")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("f"))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      writtenTokenCount++;
      token->type = StringCursorPeekStartsWith(&cursor, &StringFromLiteral("t")) ? JSON_TOKEN_BOOLEAN_TRUE
                                                                                 : JSON_TOKEN_BOOLEAN_FALSE;
      token->start = cursor.position;

      struct string *expected =
          token->type == JSON_TOKEN_BOOLEAN_FALSE ? &StringFromLiteral("false") : &StringFromLiteral("true");

      // TODO: OPTIMIZATION: Limit ExtractThrough() to expected length
      struct string got = StringCursorExtractSubstring(&cursor, expected->length);

      if (!IsStringEqual(&got, expected)) {
        parser->error = JSON_PARSER_ERROR_INVALID_BOOLEAN;
        return 0;
      } else if (StringCursorRemainingLength(&cursor) == got.length) {
        // error boolean not finished
        parser->error = JSON_PARSER_ERROR_PARTIAL;
        token->end = 0;
        cursor.position += got.length;
        break;
      }

      token->end = cursor.position + got.length;

      // Advance after boolean
      cursor.position += got.length;
    }

    // JSON_TOKEN_NUMBER
    else if (StringCursorPeekStartsWith(&cursor, &StringFromLiteral("-")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral(".")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("0")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("1")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("2")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("3")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("4")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("5")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("6")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("7")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("8")) ||
             StringCursorPeekStartsWith(&cursor, &StringFromLiteral("9"))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      struct string number = StringCursorExtractNumber(&cursor);
      token->type = JSON_TOKEN_NUMBER;
      token->start = cursor.position;
      writtenTokenCount++;

      if (StringCursorRemainingLength(&cursor) == number.length) {
        // error number not finished
        parser->error = JSON_PARSER_ERROR_PARTIAL;
        return 0;
      }

      token->end = cursor.position + number.length;

      // Advance after number
      cursor.position += number.length;
    } else {
      parser->error = JSON_PARSER_ERROR_INVALID_CHAR;
      return 0;
    }
  }

  parser->tokenCount = writtenTokenCount;
  return writtenTokenCount > 0;
}
