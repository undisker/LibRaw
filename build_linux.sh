#!/bin/bash
# LibRaw Linux Build Script
# Builds LibRaw with all dependencies for Linux x86_64

set -e

# Configuration
BUILD_TYPE="Release"
BUILD_DIR="build_linux"
OUTPUT_DIR="dependencies/bin"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "============================================"
echo "Building LibRaw Dependencies for Linux"
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
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DZLIB_BUILD_TESTING=OFF \
    ..
cmake --build . --config $BUILD_TYPE --parallel
cp -f libz.so* ../../../$OUTPUT_DIR/ 2>/dev/null || cp -f libz.a ../../../$OUTPUT_DIR/
cd ../../..

# Build libjpeg-turbo
echo ""
echo "[2/4] Building libjpeg-turbo..."
cd dependencies/libjpeg-turbo
rm -rf build && mkdir build && cd build
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DENABLE_SHARED=ON \
    -DENABLE_STATIC=OFF \
    -DWITH_TURBOJPEG=ON \
    ..
cmake --build . --config $BUILD_TYPE --parallel
cp -f libjpeg.so* ../../../$OUTPUT_DIR/
cp -f libturbojpeg.so* ../../../$OUTPUT_DIR/
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
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DBUILD_SHARED_LIBS=ON \
    ..
cmake --build . --config $BUILD_TYPE --parallel
cp -f liblcms2.so* ../../../$OUTPUT_DIR/
cd ../../..

# Build JasPer
echo ""
echo "[4/4] Building JasPer..."
cd dependencies
rm -rf jasper_build && mkdir jasper_build && cd jasper_build
cmake -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DJAS_ENABLE_SHARED=ON \
    -DJAS_ENABLE_PROGRAMS=OFF \
    -DJAS_ENABLE_DOC=OFF \
    ../jasper
cmake --build . --config $BUILD_TYPE --parallel
cp -f src/libjasper/libjasper.so* ../../$OUTPUT_DIR/
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
    -DBUILD_SHARED_LIBS=ON \
    -DLIBRAW_ENABLE_ZLIB=ON \
    -DLIBRAW_ENABLE_JPEG=ON \
    -DLIBRAW_ENABLE_LCMS2=ON \
    -DLIBRAW_ENABLE_X3FTOOLS=ON \
    -DZLIB_INCLUDE_DIR="$ZLIB_ROOT;$SCRIPT_DIR/dependencies/zlib" \
    -DZLIB_LIBRARY="$ZLIB_ROOT/libz.so" \
    -DJPEG_INCLUDE_DIR="$JPEG_ROOT;$SCRIPT_DIR/dependencies/libjpeg-turbo" \
    -DJPEG_LIBRARY="$JPEG_ROOT/libjpeg.so" \
    ..

cmake --build . --config $BUILD_TYPE --parallel

cp -f bin/libraw.so* ../$OUTPUT_DIR/
cd ..

# Set RPATH for the library
echo ""
echo "Setting RPATH..."
cd $OUTPUT_DIR
for lib in *.so*; do
    if [[ -f "$lib" && ! -L "$lib" ]]; then
        patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
    fi
done
cd ..

echo ""
echo "============================================"
echo "Build Complete!"
echo "============================================"
echo "Output files in: $OUTPUT_DIR"
ls -la $OUTPUT_DIR/*.so* 2>/dev/null || ls -la $OUTPUT_DIR/
