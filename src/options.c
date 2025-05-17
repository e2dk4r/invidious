#include "options.h"
#include "string_cursor.h"

internalfn void
OptionsInit(struct options *options)
{
  options->hostname = StringFromLiteral("i.iii.st");
  options->port = StringFromLiteral("443");
  options->videoId = StringNull();
}

internalfn b8
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
        return 0;
      }
      argumentIndex++;
      struct string instance = StringFromZeroTerminated((u8 *)arguments[argumentIndex], 1024);

      // Validation, (url, domain)

      // Write
#if 0
      options->domain = ;
      options->port = ;
#endif
    }

    else if (IsStringStartsWith(&argument, &StringFromLiteral("-h")) ||
             IsStringStartsWith(&argument, &StringFromLiteral("--help"))) {

      return 0;
    }

    else if (IsStringNull(&options->videoId)) {
      // Validation, (url, videoId)

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

      // Write
      options->videoId = videoId;
    }
  }

  return 1;
}
