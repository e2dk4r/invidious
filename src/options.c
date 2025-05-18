#include "options.h"
#include "string_cursor.h"

internalfn void
OptionsInit(struct options *options)
{
  options->hostname = StringFromLiteral("i.iii.st");
  options->port = StringFromLiteral("443");
  options->videoId = StringNull();
}

internalfn enum options_error
OptionsParse(struct options *options, u32 argumentCount, char **arguments)
{
  for (u32 argumentIndex = 1; argumentIndex < argumentCount; argumentIndex++) {
    struct string argument = StringFromZeroTerminated((u8 *)arguments[argumentIndex], 1024);
    argument = StringStripWhitespace(&argument);

    if (IsStringStartsWith(&argument, &StringFromLiteral("-i")) ||
        IsStringStartsWith(&argument, &StringFromLiteral("--instance"))) {
      // expects 1 argument
      if (argumentIndex + 1 == argumentCount) {
        // Expected instance. accepted types url domain
        return OPTIONS_ERROR_INSTANCE_REQUIRED;
      }

      argumentIndex++;
      struct string instance = StringFromZeroTerminated((u8 *)arguments[argumentIndex], 1024);

      // Validation, (url, domain)
      string domain = StringNull();
      string port = StringFromLiteral("433");

      string_cursor cursor = StringCursorFromString(&instance);

      string schemeSeparator = StringFromLiteral("://");
      string scheme = StringCursorExtractUntil(&cursor, &schemeSeparator);

      if (IsStringEqual(&scheme, &StringFromLiteral("http")))
        port = StringFromLiteral("80");
      else if (IsStringEqual(&scheme, &StringFromLiteral("https")))
        port = StringFromLiteral("433");
      else
        return OPTIONS_ERROR_INSTANCE_INVALID;

      cursor.position += scheme.length + schemeSeparator.length;

      string authority = StringCursorExtractUntilOrRest(&cursor, &StringFromLiteral("/"));
      string_cursor authorityCursor = StringCursorFromString(&authority);
      string colon = StringFromLiteral(":");
      domain = StringCursorExtractUntil(&authorityCursor, &colon);
      if (IsStringNull(&domain)) {
        domain = authority;
      } else {
        authorityCursor.position += domain.length + colon.length;
        port = StringCursorExtractRemaining(&authorityCursor);
      }

      // Write
      options->hostname = domain;
      options->port = port;
    }

    else if (IsStringStartsWith(&argument, &StringFromLiteral("-h")) ||
             IsStringStartsWith(&argument, &StringFromLiteral("--help"))) {
      return OPTIONS_ERROR_HELP;
    }

    else if (IsStringNull(&options->videoId)) {
      // video id is required

      string_cursor cursor = StringCursorFromString(&argument);

      // https://developer.mozilla.org/en-US/docs/Learn_web_development/Howto/Web_mechanics/What_is_a_URL#basics_anatomy_of_a_url
      // https://www.youtube.com/watch?v={videoId}
      // https://www.youtube.com/embed/{videoId}
      // https://youtu.be/{videoId}

      string pathSeparator = StringFromLiteral("/");
      StringCursorConsumeThroughLast(&cursor, &pathSeparator);

      string parameterKeyV = StringFromLiteral("v=");
      StringCursorConsumeThrough(&cursor, &parameterKeyV);

      string videoId = StringCursorExtractRemaining(&cursor);

      string parameterSeparator = StringFromLiteral("&");
      string beforeParameterSeparator = StringCursorExtractUntil(&cursor, &parameterSeparator);
      b8 isParameterSeperatorFound = !IsStringNull(&beforeParameterSeparator);
      if (isParameterSeperatorFound)
        videoId = beforeParameterSeparator;

      if (!isParameterSeperatorFound) {
        string anchorSeparator = StringFromLiteral("#");
        string beforeAnchorSeparator = StringCursorExtractUntil(&cursor, &anchorSeparator);
        if (!IsStringNull(&beforeAnchorSeparator))
          videoId = beforeAnchorSeparator;
      }

      // validate video id
      if (videoId.length != 11)
        return OPTIONS_ERROR_VIDEO_INVALID;
      for (u32 index = 0; index < videoId.length; index++) {
        b8 allowed[U8_MAX] = {
            // A-Z a-z 0-9 - _
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, // 0x20
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 0x30
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 0x50
            0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60
            1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // 0x70
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x80
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x90
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xa0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xb0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xc0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xd0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0xe0
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0     // 0xf0
        };
        u8 character = *(videoId.value + index);
        if (!allowed[character])
          return OPTIONS_ERROR_VIDEO_INVALID;
      }

      // Write
      options->videoId = videoId;
    }
  }

  if (IsStringNull(&options->videoId))
    return OPTIONS_ERROR_VIDEO_REQUIRED;

  return OPTIONS_ERROR_NONE;
}
