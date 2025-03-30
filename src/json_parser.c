/*
 * Inspired from https://github.com/zserge/jsmn by Serge Zaitsev
 *
 * Tokenizes json string.
 *
 * Main difference from jsmn this is intended for streaming json data.
 * (e.g. streaming http response)
 *
 * Q: Why we need this?
 * A: I do not want to write to memory by modifying HTTP 1.1 response to get
 * rid of chunk size.
 *
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
  u32 lastCursorPosition;
  u32 tokenCount;
  u32 tokenMax;
  struct json_token *tokens;
};

internalfn struct json_parser
JsonParser(struct json_token *tokens, u32 tokenMax)
{
  return (struct json_parser){
      .lastCursorPosition = 0,
      .tokenCount = 0,
      .tokenMax = tokenMax,
      .tokens = tokens,
  };
}

internalfn struct json_parser *
MakeJsonParser(memory_arena *arena, u32 tokenCount)
{
  struct json_token *tokens = MemoryArenaPushUnaligned(arena, sizeof(*tokens) * tokenCount);
  struct json_parser *parser = MemoryArenaPushUnaligned(arena, sizeof(*parser));
  parser->tokens = tokens;
  parser->tokenMax = tokenCount;
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

internalfn u32
JsonParse(struct json_parser *parser, struct string *json)
{
  u32 writtenTokenCount = 0;

  // partially written token
  enum json_token_type partialTokenType = JSON_TOKEN_NONE;
  struct json_token *partialToken = 0;
  if (parser->error == JSON_PARSER_ERROR_PARTIAL) {
    debug_assert(parser->tokenCount > 0);
    u32 partialTokenIndex = parser->tokenCount - 1;
    partialToken = JsonParserGetToken(parser, partialTokenIndex);
    partialTokenType = partialToken->type;
    debug_assert(partialToken->end == 0);
  }
  parser->error = JSON_PARSER_ERROR_NONE;

  struct string_cursor cursor = StringCursorFromString(json);
  while (!IsStringCursorAtEnd(&cursor)) {
    u32 newTokenIndex = parser->tokenCount + writtenTokenCount;
    struct json_token *newToken = JsonParserGetToken(parser, newTokenIndex);
    // b8 isPartialTokenString = 0;
    // b8 isPartialTokenBoolean = 0;
    // b8 isPartialTokenNumber = 0;
    if (partialToken) { // unlikely, accurs one time
      newToken = partialToken;
      partialToken = 0;
    }

    // JSON_TOKEN_OBJECT
    if (StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("{"))) {
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

      token->start += parser->lastCursorPosition;

      cursor.position += 1;
    }

    else if (StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("}"))) {
      struct json_token *startToken = 0;
      u32 startTokenIndex = parser->tokenCount + writtenTokenCount - 1;
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
      startToken->end += parser->lastCursorPosition;
    }

    // JSON_TOKEN_ARRAY
    else if (StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("["))) {
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

      token->start += parser->lastCursorPosition;

      cursor.position += 1;
    }

    else if (StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("]"))) {
      struct json_token *startToken = 0;
      u32 startTokenIndex = parser->tokenCount + writtenTokenCount - 1;
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
    else if (StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("\t")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("\r")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("\n")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED(" ")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED(":")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED(","))) {
      cursor.position += 1;
    }

    // JSON_TOKEN_STRING
    else if (partialTokenType == JSON_TOKEN_STRING ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("\""))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      if (partialTokenType == JSON_TOKEN_NONE) {
        writtenTokenCount++;
        cursor.position += 1;
        token->type = JSON_TOKEN_STRING;
        token->start = cursor.position;
        token->start += parser->lastCursorPosition;
      }

      struct string_cursor tempCursor = cursor;
      struct string string = StringCursorConsumeUntil(&tempCursor, &STRING_FROM_ZERO_TERMINATED("\""));
      // handle escaped double quotes
      while (IsStringEndsWith(&string, &STRING_FROM_ZERO_TERMINATED("\\"))) {
        tempCursor.position += 1; // Advance after double quotes
        string.length += 1;
        struct string nextPart = StringCursorConsumeUntil(&tempCursor, &STRING_FROM_ZERO_TERMINATED("\""));
        if (nextPart.length == 0)
          break;
        string.length += nextPart.length;
      }

      if (StringCursorRemainingLength(&cursor) == string.length) {
        // error string not closed
        parser->error = JSON_PARSER_ERROR_PARTIAL;
        token->end = 0;
        cursor.position += string.length;
        break;
      }
      token->end = cursor.position + string.length;
      token->end += parser->lastCursorPosition;

      cursor.position += string.length + 1;
    }

    // JSON_TOKEN_NULL
    else if (partialTokenType == JSON_TOKEN_NULL ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("n"))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      if (partialTokenType == JSON_TOKEN_NONE) {
        writtenTokenCount++;
        token->type = JSON_TOKEN_NULL;
        token->start = cursor.position;
        token->start += parser->lastCursorPosition;
      }

      // LEFTOFF: TODO: extract null token from json
      // "a": null, | "a":null} | "a":null | [ null]
      struct string *expected = &STRING_FROM_ZERO_TERMINATED("null");
      struct string got = StringCursorExtractSubstring(&cursor, 4);
      struct string expectedSlice = StringSlice(expected, 0, got.length);
      if (partialTokenType != JSON_TOKEN_NONE)
        expectedSlice = StringSlice(expected, expected->length - got.length, expected->length);

      if (!IsStringEqual(&got, &expectedSlice)) {
        parser->error = JSON_PARSER_ERROR_INVALID_CHAR;
        return 0;
      } else if (StringCursorRemainingLength(&cursor) == got.length) {
        // error null not finished
        parser->error = JSON_PARSER_ERROR_PARTIAL;
        token->end = 0;
        cursor.position += got.length;
        break;
      }

      token->end = cursor.position + got.length;
      token->end += parser->lastCursorPosition;

      cursor.position += got.length + 1;
    }

    // JSON_TOKEN_BOOLEAN
    else if (partialTokenType == JSON_TOKEN_BOOLEAN_FALSE || partialTokenType == JSON_TOKEN_BOOLEAN_TRUE ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("t")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("f"))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      if (partialTokenType == JSON_TOKEN_NONE) {
        writtenTokenCount++;
        token->type = StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("t")) ? JSON_TOKEN_BOOLEAN_TRUE
                                                                                             : JSON_TOKEN_BOOLEAN_FALSE;
        token->start = cursor.position;
        token->start += parser->lastCursorPosition;
      }

      b8 value = token->type == JSON_TOKEN_BOOLEAN_TRUE;
      struct string *expected =
          value == 0 ? &STRING_FROM_ZERO_TERMINATED("false") : &STRING_FROM_ZERO_TERMINATED("true");

      // TODO: OPTIMIZATION: Limit ExtractThrough() to expected length
      struct string got = StringCursorExtractThrough(&cursor, &STRING_FROM_ZERO_TERMINATED("e"));
      struct string expectedSlice = StringSlice(expected, 0, got.length);
      if (partialTokenType != JSON_TOKEN_NONE)
        expectedSlice = StringSlice(expected, expected->length - got.length, expected->length);

      if (!IsStringEqual(&got, &expectedSlice)) {
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
      token->end += parser->lastCursorPosition;

      cursor.position += got.length;
    }

    // JSON_TOKEN_NUMBER
    else if (partialTokenType == JSON_TOKEN_NUMBER ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("-")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("0")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("1")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("2")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("3")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("4")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("5")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("6")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("7")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("8")) ||
             StringCursorPeekStartsWith(&cursor, &STRING_FROM_ZERO_TERMINATED("9"))) {
      struct json_token *token = newToken;
      if (!token) {
        // out of memory
        parser->error = JSON_PARSER_ERROR_OUT_OF_TOKENS;
        return 0;
      }

      struct string number = StringCursorExtractNumber(&cursor);
      if (partialTokenType == JSON_TOKEN_NONE) {
        token->type = JSON_TOKEN_NUMBER;
        token->start = cursor.position;
        token->start += parser->lastCursorPosition;
        writtenTokenCount++;
      }

      if (StringCursorRemainingLength(&cursor) == number.length) {
        // error number not finished
        parser->error = JSON_PARSER_ERROR_PARTIAL;
        token->end = 0;
        cursor.position += number.length;
        break;
      }

      token->end = cursor.position + number.length;
      token->end += parser->lastCursorPosition;

      cursor.position += number.length;
    } else {
      parser->error = JSON_PARSER_ERROR_INVALID_CHAR;
      return 0;
    }
  }

  parser->lastCursorPosition = (u32)cursor.position;
  parser->tokenCount += writtenTokenCount;
  return writtenTokenCount;
}
