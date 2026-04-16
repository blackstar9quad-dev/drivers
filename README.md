# Linux Character Driver

A simple multi-minor character driver implemented in Linux kernel.

## Features
- Multiple minor devices
- Dynamic memory allocation
- Custom read/write operations

## Build
make

## Load
sudo insmod chardriver2.ko

## Test
echo "hello" | sudo tee /dev/character_cus_driver1
sudo cat /dev/character_cus_driver1
