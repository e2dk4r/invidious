pwd="$ProjectRoot/3rdparty/wolfssl"
outputDir="$OutputDir/3rdparty"

WOLFSSL_VERSION=5.8.0
WOLFSSL_URL="https://github.com/wolfSSL/wolfssl/archive/refs/tags/v$WOLFSSL_VERSION-stable.tar.gz"
WOLFSSL_SRC="$outputDir/wolfssl-$WOLFSSL_VERSION.tar.gz"
WOLFSSL_SRC_B2SUM=3f2dcac0d2587ecb2ce29903e358f2b432f1360b13eb69fc8e6632f8ed19e1ebd039c54806dca38c7969bb7fff2613d7d1a94b7eb70be8c70026f4f5de7f8563
WOLFSSL_DIR="$outputDir/wolfssl-$WOLFSSL_VERSION"

Log "wolfSSL:"
Log "  Version: $WOLFSSL_VERSION"

if [ ! -e "$WOLFSSL_SRC" ]; then
  StartTimer
  Log "  Starting download"
  Download "$WOLFSSL_URL" "$WOLFSSL_SRC"
  Log "  Downloaded in $(StopTimer) seconds"
fi

if [ $(HashCheckB2 "$WOLFSSL_SRC" "$WOLFSSL_SRC_B2SUM") -eq 0 ]; then
  echo "wolfSSL hash check failed"
  echo "  You need to remove '$WOLFSSL_SRC' for re-download"
  exit 1
fi

if [ ! -d "$WOLFSSL_DIR" ]; then
  StartTimer
  tar -C "$outputDir" -xf "$WOLFSSL_SRC"
  Log "  Extracted in $(StopTimer) seconds"
  if [ -d "$WOLFSSL_DIR-stable" ]; then
    mv "$WOLFSSL_DIR-stable" "$WOLFSSL_DIR"
  fi
fi

if [ ! -d "$WOLFSSL_DIR-install" ]; then
  cd "$WOLFSSL_DIR"

  ./autogen.sh
  ./configure                       \
    --prefix="$WOLFSSL_DIR-install" \
    --disable-debug                 \
    --disable-shared                \
    --disable-benchmark             \
    --disable-examples              \
    --disable-oldnames              \
    --enable-aesni-with-avx         \
    --enable-intelasm 

  StartTimer
  make install -j8
  Log "  Built in $(StopTimer) seconds"

  cd "$ProjectRoot"
fi

INC_WOLFSSL="-I$WOLFSSL_DIR-install/include"
LIB_WOLFSSL="-L$WOLFSSL_DIR-install/lib -lwolfssl"
PKGCONFIG_WOLFSSL="$WOLFSSL_DIR-install/lib/pkgconfig"
