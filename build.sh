#!/bin/bash
# Build script for ff-rknn with YOLOv8 support

set -e

# Directories and paths
TURBOJPEG_LIB="./lib1.5/libturbojpeg.a"
INCLUDE_DIRS="-I. -Iutils -I/usr/include/rga -I/usr/include/drm -I/usr/include"
DEFINES="-D_FILE_OFFSET_BITS=64 -D REENTRANT"
CFLAGS="-O2 ${INCLUDE_DIRS} ${DEFINES}"
CXXFLAGS="-O2 --permissive ${INCLUDE_DIRS} ${DEFINES}"

# SDL3 flags
SDL_FLAGS="`pkg-config --cflags --libs sdl3`"

# Libraries
LIBS="-lz -lm -lpthread -ldrm -lrockchip_mpp -lrga -lvorbis -lvorbisenc -ltiff -lopus -logg -lmp3lame -llzma -lrtmp -lssl -lcrypto -lbz2 -lxml2 -lX11 -lxcb -lXv -lXext -lv4l2 -lasound -lpulse -lGL -lGLESv2 -lsndio -lfreetype -lxcb -lxcb-shm -lxcb -lxcb-xfixes -lxcb-render -lxcb-shape -lxcb -lxcb-shape -lxcb -lavutil -lavcodec -lavformat -lavdevice -lavfilter -lswscale -lswresample -lpostproc -lrknnrt"

echo "Compiling C files..."
gcc ${CFLAGS} -c utils/file_utils.c -o file_utils.o
gcc ${CFLAGS} -c utils/image_utils.c -o image_utils.o  
gcc ${CFLAGS} -c utils/image_drawing.c -o image_drawing.o

echo "Compiling C++ files..."
g++ ${CXXFLAGS} -c utils/tracker.cc -o tracker.o
g++ ${CXXFLAGS} -c postprocess.cc -o postprocess.o
g++ ${CXXFLAGS} -c rknpu2/yolov8.cc -o yolov8.o
g++ ${CXXFLAGS} `pkg-config --cflags sdl3` -c ff-rknn.c -o ff-rknn.o

echo "Linking..."
g++ -O2 -o ff-rknn ff-rknn.o postprocess.o yolov8.o image_drawing.o image_utils.o file_utils.o tracker.o \
    ${SDL_FLAGS} ${LIBS} ${TURBOJPEG_LIB}

echo "Cleaning up object files..."
rm -f *.o

echo "Build successful! Binary: ff-rknn"
ls -lh ff-rknn
