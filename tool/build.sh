# vi: set et ft=sh ts=2 sw=2 fenc=utf-8 :vi

pwd="$ProjectRoot/tool"
outputDir="$OutputDir/tool"
if [ ! -e "$outputDir" ]; then
  mkdir "$outputDir"
fi

### gen_pseudo_random
inc="-I$ProjectRoot/include"
src="$pwd/gen_pseudo_random.c"
lib=""
output="$outputDir/$(BasenameWithoutExtension "$src")"
"$cc" $cflags $ldflags $inc -o "$output" $src $lib
