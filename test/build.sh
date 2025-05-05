# vi: set et ft=sh ts=2 sw=2 fenc=utf-8 :vi
################################################################
# TEST FUNCTIONS
################################################################

failedTestCount=0

# void RunTest(testExecutable, failMessage)
RunTest() {
  executable="$1"
  failMessage="$2"

  "$executable"
  statusCode=$?
  if [ $statusCode -ne 0 ]; then
    echo "$failMessage code $statusCode"
    failedTestCount=$(( $failedTestCount + 1 ))
  fi
}

################################################################

pwd="$ProjectRoot/test"
outputDir="$OutputDir/test"
if [ ! -e "$outputDir" ]; then
  mkdir "$outputDir"
fi

LIB_M='-lm'

### json_parser
inc="-I$ProjectRoot/include -I$ProjectRoot/src"
src="$pwd/json_parser_test.c"
output="$outputDir/$(BasenameWithoutExtension "$src")"
lib=""
"$cc" $cflags $ldflags $inc -o "$output" $src $lib
RunTest "$output" "TEST json parser failed."

### http_parser
inc="-I$ProjectRoot/include -I$ProjectRoot/src"
src="$pwd/http_parser_test.c"
output="$outputDir/$(BasenameWithoutExtension "$src")"
lib=""
"$cc" $cflags $ldflags $inc -o "$output" $src $lib
RunTest "$output" "TEST http parser failed."

if [ $failedTestCount -ne 0 ]; then
  echo $failedTestCount tests failed.
  exit 1
fi
