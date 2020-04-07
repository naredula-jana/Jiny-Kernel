#!/usr/bin/env bash
cd /run
wget --no-check-certificate https://github.com/naredula-jana/Jiny-Kernel/blob/master/bin/disk.img
exec qemu-system-x86_64 -m 1024M  -smp 1 -vnc :8  -hda ./disk.img -drive if=virtio,id=hdr0,file=./addon_disk -nographic  \
    "$@"
