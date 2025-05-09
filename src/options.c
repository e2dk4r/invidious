#include "options.h"

internalfn b8
OptionsParse(struct options *options, u32 argumentCount, char **arguments)
{
  for (u32 argumentIndex = 1; argumentIndex < argumentCount; argumentIndex++) {
    struct string argument = StringFromZeroTerminated((u8 *)(arguments + argumentIndex), 1024);
    argument = StringStripWhitespace(argument);

    if (IsStringStartWith(&argument, &StringFromLiteral("-i"))
            IsStringStartWith(&argument, &StringFromLiteral("--instance"))) {
      // expects 1 argument
      if (argumentIndex + 1 == argumentCount) {
        // Expected instance. accepted types url domain
        return 0;
      }
      argumentIndex++;
      struct string instance = StringFromZeroTerminated((u8 *)(arguments + argumentIndex), 1024);

      // Validation, (url, domain)
      // Write
      options->domain = ;
      options->port = ;
    }

    else if (IsStringNull(&options->videoId)) {
      // Validation, (url, videoId)

      string_cursor cursor = StringCursorFromString(argument);

      // https://developer.mozilla.org/en-US/docs/Learn_web_development/Howto/Web_mechanics/What_is_a_URL#basics_anatomy_of_a_url
      // https://www.youtube.com/watch?v={videoId}
      // https://www.youtube.com/embed/{videoId}
      // https://youtu.be/{videoId}

      struct string pathSeparator = StringFromLiteral("/");
      // StringCursorAdvanceAfterLast(&cursor, &pathSeparator);
      if (StringCursorConsumeUntilLast(&cursor, &pathSeparator).length != 0)
        cursor.position += pathSeperator.length;

      struct string parameterKeyV = StringFromLiteral("v=");
      // StringCursorAdvanceAfterLast(&cursor, &parameterKeyV);
      struct string beforeParameterV = StringCursorExtractUntil(cursor, &parameterKeyV);
      if (beforeParameterV.length != 0)
        cursor.position += beforeParameterV.length + parameterKeyV.length;

      struct string videoId = StringCursorExtractRemaining(cursor);

      struct string parameterSeparator = StringFromLiteral("&");
      struct string beforeParameterSeparator = StringCursorExtractUntil(cursor, &parameterSeparator);
      if (!IsStringNull(&beforeParameterSeparator))
        videoId = beforeParameterSeparator;

      struct string anchorSeparator = StringFromLiteral("#");
      struct string beforeAnchorSeparator = StringCursorExtractUntil(cursor, &anchorSeparator);
      if (!IsStringNull(&beforeAnchorSeparator))
        videoId = beforeAnchorSeparator;

      // Write
      options->videoId = videoId;
    }
  }
  return 0;

  return 1;
}
