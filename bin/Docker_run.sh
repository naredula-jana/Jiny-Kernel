#!/usr/bin/env bash

exec qemu-system-x86_64 -m 1024M  -smp 1 -vnc :8  -hda /run/disk.img -drive if=virtio,id=hdr0,file=/run/addon_disk -nographic  \
    "$@" 
