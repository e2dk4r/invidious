outputDir="$OutputDir/3rdparty"
if [ ! -d "$outputDir" ]; then
  mkdir "$outputDir"
fi

# TLS LIBRARY
## WITH_MBEDTLS=
## WITH_WOLFSSL=

if [ $(( $WITH_MBEDTLS + $WITH_WOLFSSL )) -eq 0 ]; then
  echo 'Expected to have one TLS library selected.'
  exit 1
elif [ $(( $WITH_MBEDTLS + $WITH_WOLFSSL )) -gt 1 ]; then
  echo 'Select only one TLS library. You selected:'
  [ $WITH_MBEDTLS -eq 1 ] && echo ' - mbedTLS'
  [ $WITH_WOLFSSL -eq 1 ] && echo ' - wolfSSL'
  exit 1
fi

pwd="$ProjectRoot/3rdparty"
if [ $WITH_MBEDTLS -eq 1 ]; then
  . "$pwd/mbedtls/build.sh"
fi

pwd="$ProjectRoot/3rdparty"
if [ $WITH_WOLFSSL -eq 1 ]; then
  . "$pwd/wolfssl/build.sh"
fi
