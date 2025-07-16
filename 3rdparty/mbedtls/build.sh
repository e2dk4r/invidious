pwd="$ProjectRoot/3rdparty/mbedtls"
outputDir="$OutputDir/3rdparty"

MBEDTLS_VERSION=3.6.4
MBEDTLS_URL="https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-$MBEDTLS_VERSION/mbedtls-$MBEDTLS_VERSION.tar.bz2"
MBEDTLS_DIR="$outputDir/mbedtls-$MBEDTLS_VERSION"
MBEDTLS_SRC="$outputDir/mbedtls-$MBEDTLS_VERSION.tar.bz2"
MBEDTLS_SRC_B2SUM=d9bfb3984081a346ac5b022af79f133a0e97cd17770e3aa468d0db200cebd21f92d00cddbe23038fbbda61deb1d9d96f5ac84b0b3a75e0e98b36d236f97e738a

Log "Mbed TLS:"
Log "  Version: $MBEDTLS_VERSION"

isMbedtlsBuilt=0
if [ -d "$MBEDTLS_DIR-install" ] && [ "$FORCE_BUILD_MBEDTLS" -eq 0 ]; then
  isMbedtlsBuilt=1
fi

if [ $isMbedtlsBuilt -eq 0 ]; then
  if [ ! -e "$MBEDTLS_SRC" ]; then
    StartTimer
    Log "  Starting download"
    Download "$MBEDTLS_URL" "$MBEDTLS_SRC"
    Log "  Downloaded in $(StopTimer) seconds"
  fi

  if [ $(HashCheckB2 "$MBEDTLS_SRC" "$MBEDTLS_SRC_B2SUM" ) -eq 0 ]; then
    echo "Mbed TLS hash check failed"
    echo "  You need to remove '$MBEDTLS_SRC' for re-download"
    exit 1
  fi

  if [ ! -d "$MBEDTLS_DIR" ]; then
    StartTimer
    tar -C "$outputDir" -xf "$MBEDTLS_SRC"
    Log "  Extracted in $(StopTimer) seconds"
  fi

  # TODO: Reduce mbedtls memory footprint
  # https://mbed-tls.readthedocs.io/en/latest/kb/how-to/reduce-polarssl-memory-and-storage-footprint/

  # copy config
  # https://mbed-tls.readthedocs.io/en/latest/kb/compiling-and-building/how-do-i-configure-mbedtls/
  # https://mbed-tls.readthedocs.io/en/latest/kb/how-to/using-static-memory-instead-of-the-heap/
  cp "$pwd/mbedtls_config.h" "$MBEDTLS_DIR/include/mbedtls/mbedtls_config.h"

  # README.md > ### CMake
  mbedtlsBuildType="Debug"
  if [ $IsBuildDebug -eq 0 ]; then
    mbedtlsBuildType="Release"
  fi
  StartTimer
  cmake -G Ninja -S "$MBEDTLS_DIR" -B "$MBEDTLS_DIR/build" --install-prefix "$MBEDTLS_DIR-install" -D CMAKE_BUILD_TYPE="$mbedtlsBuildType" \
    -D ENABLE_PROGRAMS=OFF \
    -D ENABLE_TESTING=OFF \
    -D IS_PLATFORM_LINUX="$IsPlatformLinux" \
    -D IS_PLATFORM_WINDOWS="$IsPlatformWindows"
  Log "  Configured in $(StopTimer) seconds"

  StartTimer
  ninja -C "$MBEDTLS_DIR/build" install
  Log "  Built in $(StopTimer) seconds"
fi

INC_MBEDTLS="-I$MBEDTLS_DIR-install/include"
LIB_MBEDTLS="-L$MBEDTLS_DIR-install/lib64 -lmbedtls -lmbedx509 -lmbedcrypto"
PKGCONFIG_MBEDTLS="$MBEDTLS_DIR-install/lib64/pkgconfig"
