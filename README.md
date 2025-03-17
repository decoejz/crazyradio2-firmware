# Crazyradio 2.0 Firmware  [![CI](https://github.com/bitcraze/crazyradio2-firmware/workflows/CI/badge.svg)](https://github.com/bitcraze/crazyradio2-firmware/actions?query=workflow%3ACI)

This project contains the source code for the firmware used in the Crazyradio 2.0.

## Building and Flashing

See the [building and flashing instructions](./docs/building-and-flashing/index.md) in the docs folder.


## Official Documentation

Check out the [Bitcraze crazyradio2-firmware documentation](https://www.bitcraze.io/documentation/repository/crazyradio2-firmware/main/) on our website.


## Contribute

Go to the [contribute page](https://www.bitcraze.io/contribute/) on our website to learn more.


## License

The code is licensed under the MIT license
####################################
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.4/zephyr-sdk-0.16.4_linux-x86_64.tar.xz

tar -xvf zephyr-sdk-0.16.4_linux-x86_64.tar.xz
sudo mv zephyr-sdk-0.16.4 /opt/zephyr-sdk

cd /opt/zephyr-sdk
./setup.sh

echo 'export ZEPHYR_TOOLCHAIN_VARIANT=zephyr' >> ~/.bashrc
echo 'export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk' >> ~/.bashrc
echo 'export ZEPHYR_BASE=~/workspace/ita/andre/crazyradio2-firmware/zephyr' >> ~/.bashrc

export DTC_OVERLAY_FILE=$ZEPHYR_SDK_INSTALL_DIR/sysroots/x86_64-pokysdk-linux/usr/bin/dtc


source ~/.bashrc

west init
west update

west zephyr-export

sudo apt update && sudo apt install --no-install-recommends \
    cmake ninja-build gperf ccache dfu-util \
    device-tree-compiler wget \
    python3-pip python3-setuptools \
    python3-tk python3-wheel xz-utils file \
    make gcc git unzip tar udev

pip install -U west
pip install -r ./zephyr/scripts/requirements.txt

west build -b crazyradio2

west build -b bitcraze_crazyradio_2 --pristine
