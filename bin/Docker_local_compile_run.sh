#!/usr/bin/env bash
cd /opt/jiny_src
gcc --version
make all
cd bin
sudo umount /dev/loop1
sudo losetup -d /dev/loop1
losetup
sudo losetup /dev/loop1 ./disk.img -o 1048576
losetup 
mkdir /opt/mnt/
sudo mount /dev/loop1 /opt/mnt
sudo cp /opt/jiny_src/bin/jiny_image.bin /opt/mnt/boot/jiny_kernel
sudo umount ./mnt
ls -al
exec qemu-system-x86_64 -m 1024M  -smp 1 -vnc :8  -hda ./disk.img -drive if=virtio,id=hdr0,file=./addon_disk -nographic  \
    "$@"
