#!/usr/bin/env bash
cd /opt/jiny_src
if [ -f /opt/jiny_src/bin/jiny_image.bin ]; then
    echo "image file found "
else 
    echo "image file not FOUIND, pls compile ."
    exit
fi
cd bin
exec qemu-system-x86_64 -m 1024M  -smp 1 -vnc :8  -hda ./disk.img -drive if=virtio,id=hdr0,file=./addon_disk -nographic  "$@"

