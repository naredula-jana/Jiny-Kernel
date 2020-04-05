#!/usr/bin/env bash
cd /
tar xvzf include.tar.gz
cd /opt/
tar xvzf gcc-4.8.2.tar.gz
wget --no-check-certificate https://github.com/naredula-jana/Jiny-Kernel/archive/master.zip
unzip master.zip
cd Jiny-Kernel-master
gcc --version
make all
cd bin
mkdir mnt
chmod +x ./update_image
./update_image
exec qemu-system-x86_64 -m 1024M  -smp 1 -vnc :8  -hda ./disk.img -drive if=virtio,id=hdr0,file=./addon_disk -nographic  \
    "$@"
