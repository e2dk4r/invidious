outputDir="$OutputDir/3rdparty"
if [ ! -d "$outputDir" ]; then
  mkdir "$outputDir" 
fi

MBEDTLS_VERSION=3.6.2
MBEDTLS_DIR="$outputDir/mbedtls-$MBEDTLS_VERSION"

Log "Mbed TLS:"
Log "  Version: $MBEDTLS_VERSION"

isMbedtlsBuilt=0
if [ -d "$MBEDTLS_DIR-install" ] && [ "$FORCE_BUILD_MBEDTLS" -eq 0 ]; then
  isMbedtlsBuilt=1
fi

if [ $isMbedtlsBuilt -eq 0 ]; then
  StartTimer
  Log "  Starting download"
  Download "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_VERSION/mbedtls-$MBEDTLS_VERSION.tar.bz2" "$outputDir/mbedtls-$MBEDTLS_VERSION.tar.bz2"
  Log "  Downloaded in $(StopTimer) seconds"

  if [ $(HashCheckB2 "$outputDir/mbedtls-$MBEDTLS_VERSION.tar.bz2" dbf34ca3cffca7a9bdb10191bd58971583ae3f2cdef3e350ccda08eae2e7b52f5fd4d1aff5582ee120b6e35e6843d7dd323ba7da5f1428c16130e5ed7c0d689e) -eq 0 ]; then
    echo "Mbed TLS hash check failed"
    exit 1
  fi

  StartTimer
  tar --cd "$outputDir" --extract --file "$outputDir/mbedtls-$MBEDTLS_VERSION.tar.bz2"
  Log "  Extracted in $(StopTimer) seconds"

  # TODO: Configure Mbed TLS
  # https://mbed-tls.readthedocs.io/en/latest/kb/compiling-and-building/how-do-i-configure-mbedtls/
  # https://mbed-tls.readthedocs.io/en/latest/kb/how-to/using-static-memory-instead-of-the-heap/
  # https://mbed-tls.readthedocs.io/en/latest/kb/how-to/reduce-polarssl-memory-and-storage-footprint/

  # README.md > ### CMake
  mbedtlsBuildType="Debug"
  if [ $IsBuildDebug -eq 0 ]; then
    mbedtlsBuildType="Release"
  fi
  StartTimer
  cmake -G Ninja -S "$MBEDTLS_DIR" -B "$MBEDTLS_DIR/build" --install-prefix "$MBEDTLS_DIR-install" -D CMAKE_BUILD_TYPE="$mbedtlsBuildType" \
    -D ENABLE_PROGRAMS=OFF \
    -D ENABLE_TESTING=OFF
  Log "  Configured in $(StopTimer) seconds"

  StartTimer
  ninja -C "$MBEDTLS_DIR/build" install 
  Log "  Built in $(StopTimer) seconds"
fi

INC_MBEDTLS="-I$MBEDTLS_DIR-install/include"
LIB_MBEDTLS="-L$MBEDTLS_DIR-install/lib64 -lmbedtls -lmbedx509 -lmbedcrypto"
PKGCONFIG_MBEDTLS="$MBEDTLS_DIR-install/lib64/pkgconfig"
