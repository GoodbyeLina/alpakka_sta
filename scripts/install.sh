# SPDX-License-Identifier: GPL-2.0-only
# Copyright (C) 2022, Input Labs Oy.

# Pico SDK.
SDK_URL=https://github.com/raspberrypi/pico-sdk.git
SDK_TAG=1.5.1

# ARM toolchain.
# WEBSITE: https://developer.arm.com/downloads/-/gnu-rm
ARM_URL_COMMON=https://developer.arm.com/-/media/Files/downloads/gnu/12.3.rel1/binrel
ARM_FILENAME_DARWIN=arm-gnu-toolchain-12.3.rel1-darwin-arm64-arm-none-eabi.tar.xz
ARM_FILENAME_LINUX_X86_64=arm-gnu-toolchain-12.3.rel1-x86_64-arm-none-eabi.tar.xz
ARM_FILENAME_LINUX_ARM64=arm-gnu-toolchain-12.3.rel1-aarch64-arm-none-eabi.tar.xz
ARM_TAR=arm-toolchain.tar.bz2
ARM_DIR=arm-toolchain


# Do not remove this.
## GITHUB IMPORTS ENVS UNTIL HERE

rm -rf deps
mkdir deps
cd deps

PLATFORM="$( uname -sm )"

if [ "$PLATFORM" = "Darwin arm64" ]; then
    ARM_URL=$ARM_URL_COMMON/$ARM_FILENAME_DARWIN
elif [ "$PLATFORM" = "Linux x86_64" ]; then
    ARM_URL=$ARM_URL_COMMON/$ARM_FILENAME_LINUX_X86_64
elif [ "$PLATFORM" = "Linux aarch64" ]; then
    ARM_URL=$ARM_URL_COMMON/$ARM_FILENAME_LINUX_ARM64
else
    echo "Unsupported platform: ${PLATFORM}"
    exit 1
fi

echo 'Downloading ARM toolchain...'
echo $ARM_URL
curl --progress-bar -L -o $ARM_TAR $ARM_URL

echo 'Extracting ARM toolchain...'
mkdir $ARM_DIR
tar -xf $ARM_TAR --directory $ARM_DIR --strip-components 1
rm $ARM_TAR

echo "Downloading Pico C SDK..."

# 这个install文件只写了从github上拉取SDK，我的电脑访问外网速度很慢，所以写了一个拉取本地的。
# 检查PICO_SDK_PATH环境变量是否设置
# if [ -n "$PICO_SDK_PATH" ]; then
#     echo "Using local Pico SDK from environment variable PICO_SDK_PATH: $PICO_SDK_PATH"
#     cd $PICO_SDK_PATH
#     echo "Configuring Pico C SDK..."
#     git submodule update --init
#     echo "Dependencies installed"
#     exit 0
# fi



git clone $SDK_URL
cd pico-sdk
git checkout --quiet $SDK_TAG

echo "Configuring Pico C SDK..."
git submodule update --init

echo "Dependencies installed"
