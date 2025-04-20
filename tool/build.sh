# vi: set et ft=sh ts=2 sw=2 fenc=utf-8 :vi

pwd="$ProjectRoot/tool"
outputDir="$OutputDir/tool"
if [ ! -e "$outputDir" ]; then
  mkdir "$outputDir"
fi

### gen_pseudo_random
tool=gen_pseudo_random
inc="-I$ProjectRoot/include"
src="$pwd/$tool.c"
lib=""
output="$outputDir/$tool"
StartTimer
if "$cc" $cflags $ldflags $inc -o "$output" $src $lib; then
  echo "$tool compiled in $(StopTimer) seconds."
fi
