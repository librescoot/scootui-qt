# armhf toolchain for the DBC (i.MX6DL, Cortex-A9). Used by cross-build.sh
# inside the scootui-crossbuild image, where the armhf sysroot is the image
# root itself: Debian multiarch, not a separate tree.
#
# docker/Dockerfile.crossbuild repeats these settings on its QMapLibre configure
# line because this file is not in that build context. Keep the two in step.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_LIBRARY_ARCHITECTURE arm-linux-gnueabihf)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard")

# Target packages come from the multiarch paths; host tools (moc, rcc, qmlcachegen,
# qsb) are native amd64 and must not be searched for under the target paths.
set(CMAKE_FIND_ROOT_PATH /usr/lib/arm-linux-gnueabihf /usr)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

set(CMAKE_PREFIX_PATH /usr)
set(QT_HOST_PATH /usr)

# Without this, pkg-config answers for the host: hiredis and libzstd would
# resolve to the amd64 copies and fail at link.
set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig")
