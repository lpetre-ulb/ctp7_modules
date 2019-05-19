set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fomit-frame-pointer -pipe -fno-common -fno-builtin -Wall -march=armv7-a -mfpu=neon -mfloat-abi=hard -mthumb-interwork -mtune=cortex-a9 -DEMBED -Dlinux -D__linux__ -Dunix")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fomit-frame-pointer -pipe -fno-common -fno-builtin -Wall -march=armv7-a -mfpu=neon -mfloat-abi=hard -mthumb-interwork -mtune=cortex-a9 -DEMBED -Dlinux -D__linux__ -Dunix")

set(CMAKE_SYSROOT ${PETA_SYSROOT})
set(CMAKE_STAGING_PREFIX ${PETA_STAGING})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
