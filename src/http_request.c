#pragma once

#include "memory.h"
#include "text.h"

enum http_method {
  HTTP_METHOD_GET,
  HTTP_METHOD_HEAD,
  HTTP_METHOD_POST,
  HTTP_METHOD_PUT,
  HTTP_METHOD_DELETE,
};

enum http_version {
  HTTP_VERSION_10,
  HTTP_VERSION_11,
  HTTP_VERSION_20,
};

struct http_request_builder {
  enum http_method method;
  enum http_version version;
};

enum http_encoding {
  HTTP_ENCODING_NONE = 0,
  HTTP_ENCODING_GZIP,
  HTTP_ENCODING_BROTLI,
  HTTP_ENCODING_ZSTD,
};

enum http_content_type {
  HTTP_CONTENT_TYPE_NONE = 0,
  HTTP_CONTENT_TYPE_JSON,
  HTTP_CONTENT_TYPE_FORM_URLENCODED,
};

struct http_form_urlencoded_item {
  struct string name;
  struct string value;
};

struct http_form_urlencoded_list {
  u32 itemCount;
  struct http_form_urlencoded_item *items;
};

struct http_header {
  struct string name;
  struct string value;
};

struct http_request_info {
  enum http_method method;
  enum http_version version;
  struct string host;
  struct string path;
  struct string url;
  struct string userAgent;
  u32 headerCount;
  struct http_header *headers;
  enum http_content_type accept;
  enum http_encoding acceptEncoding;
  enum http_content_type contentType;
  enum http_encoding contentEncoding;

  /* if contentType is:
   *   - `HTTP_CONTENT_TYPE_JSON` type must be `struct string *`
   *   - `HTTP_CONTENT_TYPE_FORM_URLENCODED` type must be `struct http_form_urlencoded_list *`
   */
  void *content;
};

#include "string_builder.h"

/*
 * Builds http request to a buffer
 * @return http request
 *         null if any error happened
 */
struct string
HttpRequestBuild(struct http_request_info *info, struct memory_arena *memory)
{
  struct string result = StringNull();

  if (IsStringNullOrEmpty(&info->url) && IsStringNullOrEmpty(&info->host))
    return result;

  struct string SPACE = StringFromLiteral(" ");
  struct string CRLF = StringFromLiteral("\r\n");

  struct string host = info->host;
  struct string path = info->path;
  if (IsStringNullOrEmpty(&host)) {
    struct string_cursor cursor = StringCursorFromString(&info->url);
    struct string schemeSeparator = StringFromLiteral("://");
    struct string scheme = StringCursorConsumeThrough(&cursor, &schemeSeparator);
    if (IsStringNull(&scheme))
      return result;

    struct string pathSeparator = StringFromLiteral("/");
    host = StringCursorConsumeUntilOrRest(&cursor, &pathSeparator);

    path = StringCursorExtractRemaining(&cursor);
  }
  if (IsStringNull(&path))
    path = StringFromLiteral("/");

  struct string_builder *sb = MakeStringBuilder(memory, 1024, 0);

  { // Request-Line

    // Method
    switch (info->method) {
    case HTTP_METHOD_GET: {
      StringBuilderAppendStringLiteral(sb, "GET");
    } break;
    case HTTP_METHOD_HEAD: {
      StringBuilderAppendStringLiteral(sb, "HEAD");
    } break;
    case HTTP_METHOD_POST: {
      StringBuilderAppendStringLiteral(sb, "POST");
    } break;
    case HTTP_METHOD_PUT: {
      StringBuilderAppendStringLiteral(sb, "PUT");
    } break;
    case HTTP_METHOD_DELETE: {
      StringBuilderAppendStringLiteral(sb, "DELETE");
    } break;
    }

    // Request-URI
    StringBuilderAppendString(sb, &SPACE);
    StringBuilderAppendString(sb, &path);

    // HTTP-Version
    StringBuilderAppendString(sb, &SPACE);

    switch (info->version) {
    case HTTP_VERSION_10: {
      return result;
#if 0
      StringBuilderAppendStringLiteral(sb, "HTTP/1.0");
#endif
    } break;
    case HTTP_VERSION_11: {
      StringBuilderAppendStringLiteral(sb, "HTTP/1.1");
    } break;
    case HTTP_VERSION_20: {
      return result;
#if 0
      StringBuilderAppendStringLiteral(sb, "HTTP/2.0");
#endif
    } break;
    }
  }

  StringBuilderAppendString(sb, &CRLF);

  // host
  StringBuilderAppendStringLiteral(sb, "host:");
  StringBuilderAppendString(sb, &host);
  StringBuilderAppendString(sb, &CRLF);

  // headers:
  for (u32 headerIndex = 0; headerIndex < info->headerCount; headerIndex++) {
    struct http_header *header = info->headers + headerIndex;

    StringBuilderAppendString(sb, &header->name);
    StringBuilderAppendStringLiteral(sb, ":");
    StringBuilderAppendString(sb, &header->value);

    StringBuilderAppendString(sb, &CRLF);
  }

  if (info->content) { // entity-header
    struct memory_temp tempMemory = MemoryTempBegin(memory);

    struct string content = StringNull();
    if (info->contentType == HTTP_CONTENT_TYPE_FORM_URLENCODED) {
      /*
       * Content body contains the form data in key=value pairs, with each pair
       * separated by an & symbol.
       * Ref: https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Methods/POST#url-encoded_form_submission
       */

      struct string_builder *contentBuilder = MakeStringBuilder(tempMemory.arena, 2048, 0);
      struct http_form_urlencoded_list *list = info->content;

      struct string pairSeparator = StringFromLiteral("=");
      struct string separator = StringFromLiteral("&");
      for (u32 itemIndex = 0; itemIndex < list->itemCount; itemIndex++) {
        struct http_form_urlencoded_item *item = list->items + itemIndex;

        StringBuilderAppendString(contentBuilder, &item->name);
        StringBuilderAppendString(contentBuilder, &pairSeparator);
        StringBuilderAppendString(contentBuilder, &item->value);
        if (itemIndex + 1 != list->itemCount) {
          StringBuilderAppendString(contentBuilder, &separator);
        }
      }

      content = StringBuilderFlush(contentBuilder);
    } else if (info->contentType == HTTP_CONTENT_TYPE_JSON) {
      content = *(struct string *)info->content;
    }

    // content-type:
    StringBuilderAppendStringLiteral(sb, "content-type:");
    switch (info->contentType) {
    case HTTP_CONTENT_TYPE_JSON: {
      StringBuilderAppendStringLiteral(sb, "application/json");
    } break;
    case HTTP_CONTENT_TYPE_FORM_URLENCODED: {
      StringBuilderAppendStringLiteral(sb, "application/x-www-form-urlencoded");
    } break;
    default:
      break;
    }
    StringBuilderAppendString(sb, &CRLF);

    // TODO: content-encoding: encode content in specified compression
    if (info->contentEncoding == HTTP_ENCODING_GZIP) {
    }

    // content-length:
    StringBuilderAppendStringLiteral(sb, "content-length:");
    struct string *contentLengthStringBuffer = MakeString(tempMemory.arena, 32);
    struct string contentLengthString = FormatU64(contentLengthStringBuffer, content.length);
    StringBuilderAppendString(sb, &contentLengthString);
    StringBuilderAppendString(sb, &CRLF);

    StringBuilderAppendString(sb, &CRLF);

    // entity body
    StringBuilderAppendString(sb, &content);

    MemoryTempEnd(&tempMemory);
  } else {
    StringBuilderAppendString(sb, &CRLF);
  }

  result = StringBuilderFlush(sb);
  return result;
}
