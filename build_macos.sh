#!/bin/bash
# LibRaw macOS Build Script
# Builds LibRaw with all dependencies for macOS (Universal Binary: x86_64 + arm64)

set -e

# Configuration
BUILD_TYPE="Release"
BUILD_DIR="build_macos"
OUTPUT_DIR="dependencies/bin"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# macOS deployment target
export MACOSX_DEPLOYMENT_TARGET="10.15"

# Build for Universal Binary (x86_64 + arm64) if on Apple Silicon, otherwise just native
if [[ $(uname -m) == "arm64" ]]; then
    CMAKE_OSX_ARCH="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
else
    CMAKE_OSX_ARCH=""
fi

echo "============================================"
echo "Building LibRaw Dependencies for macOS"
echo "============================================"

cd "$SCRIPT_DIR"

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Build zlib
echo ""
echo "[1/4] Building zlib..."
cd dependencies/zlib
rm -rf build && mkdir build && cd build
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    $CMAKE_OSX_ARCH \
    -DZLIB_BUILD_TESTING=OFF \
    ..
cmake --build . --config $BUILD_TYPE --parallel
cp -f libz*.dylib ../../../$OUTPUT_DIR/ 2>/dev/null || cp -f libz.a ../../../$OUTPUT_DIR/
cd ../../..

# Build libjpeg-turbo
echo ""
echo "[2/4] Building libjpeg-turbo..."
cd dependencies/libjpeg-turbo
rm -rf build && mkdir build && cd build
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    $CMAKE_OSX_ARCH \
    -DENABLE_SHARED=ON \
    -DENABLE_STATIC=OFF \
    -DWITH_TURBOJPEG=ON \
    ..
cmake --build . --config $BUILD_TYPE --parallel
cp -f libjpeg*.dylib ../../../$OUTPUT_DIR/
cp -f libturbojpeg*.dylib ../../../$OUTPUT_DIR/
# Copy generated headers
cp -f jconfig.h ../
cp -f jconfigint.h ../
cd ../../..

# Build LCMS2
echo ""
echo "[3/4] Building LCMS2..."
cd dependencies/lcms2
rm -rf build && mkdir build && cd build
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    $CMAKE_OSX_ARCH \
    -DBUILD_SHARED_LIBS=ON \
    ..
cmake --build . --config $BUILD_TYPE --parallel
cp -f liblcms2*.dylib ../../../$OUTPUT_DIR/
cd ../../..

# Build JasPer
echo ""
echo "[4/4] Building JasPer..."
cd dependencies
rm -rf jasper_build && mkdir jasper_build && cd jasper_build
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    $CMAKE_OSX_ARCH \
    -DJAS_ENABLE_SHARED=ON \
    -DJAS_ENABLE_PROGRAMS=OFF \
    -DJAS_ENABLE_DOC=OFF \
    ../jasper
cmake --build . --config $BUILD_TYPE --parallel
cp -f src/libjasper/libjasper*.dylib ../../$OUTPUT_DIR/
cd ../..

# Build LibRaw
echo ""
echo "============================================"
echo "Building LibRaw..."
echo "============================================"
rm -rf $BUILD_DIR && mkdir $BUILD_DIR && cd $BUILD_DIR

# Set paths to dependencies
ZLIB_ROOT="$SCRIPT_DIR/dependencies/zlib/build"
JPEG_ROOT="$SCRIPT_DIR/dependencies/libjpeg-turbo/build"
LCMS2_ROOT="$SCRIPT_DIR/dependencies/lcms2/build"

cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    $CMAKE_OSX_ARCH \
    -DBUILD_SHARED_LIBS=ON \
    -DLIBRAW_ENABLE_ZLIB=ON \
    -DLIBRAW_ENABLE_JPEG=ON \
    -DLIBRAW_ENABLE_LCMS2=ON \
    -DLIBRAW_ENABLE_X3FTOOLS=ON \
    -DZLIB_INCLUDE_DIR="$ZLIB_ROOT;$SCRIPT_DIR/dependencies/zlib" \
    -DZLIB_LIBRARY="$ZLIB_ROOT/libz.dylib" \
    -DJPEG_INCLUDE_DIR="$JPEG_ROOT;$SCRIPT_DIR/dependencies/libjpeg-turbo" \
    -DJPEG_LIBRARY="$JPEG_ROOT/libjpeg.dylib" \
    ..

cmake --build . --config $BUILD_TYPE --parallel

cp -f bin/libraw*.dylib ../$OUTPUT_DIR/
cd ..

# Fix library paths with install_name_tool
echo ""
echo "Fixing library paths..."
cd $OUTPUT_DIR
for lib in *.dylib; do
    if [[ -f "$lib" ]]; then
        install_name_tool -id "@rpath/$lib" "$lib" 2>/dev/null || true
    fi
done
cd ..

echo ""
echo "============================================"
echo "Build Complete!"
echo "============================================"
echo "Output files in: $OUTPUT_DIR"
ls -la $OUTPUT_DIR/*.dylib 2>/dev/null || ls -la $OUTPUT_DIR/
