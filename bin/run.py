#!/usr/bin/env python2
"""
/*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
*   run.py
*   Naredula Janardhana Reddy  (naredula.jana@gmail.com, naredula.jana@yahoo.com)
*
*/
"""
import subprocess
import sys
import argparse
import os
import tempfile
import errno

"""
sudo ./qemu-system-x86_64 -gdb tcp::1336 -m 1024M -hda /home/jana/jiny/bin/new_bootdisk 
-hdb /home/jana/jiny/bin/g2_image -enable-kvm   -vnc :8
-netdev tap,id=guest0,vhost=on,vhostforce 
-smp 4 -device virtio-net-pci,mac=00:30:48:DB:5E:06,netdev=guest0 -serial telnet::50008,server,nowait
-fsdev local,security_model=passthrough,id=fsdev0,path=/opt_src/Jiny-Kernel/test/root/ 
-device virtio-9p-pci,id=fs0,fsdev=fsdev0,mount_tag=hostshare
-monitor tcp::51008,server,nowait,nodelay -balloon virtio
"""

stty_params=None

devnull = open('/dev/null', 'w')
def stty_save():
    global stty_params
    p = subprocess.Popen(["stty", "-g"], stdout=subprocess.PIPE, stderr=devnull)
    stty_params, err = p.communicate()
    stty_params = stty_params.strip()
    
def stty_restore():
    if (stty_params):
        subprocess.call(["stty", stty_params], stderr=devnull)
        
def start_jiny(options):
    gdb_port = 1335 + int(options.vm_instance)
    monitor_port = 51007 + int(options.vm_instance)
    vnc_port = 7 + int(options.vm_instance)
    args = [
        "/opt/qemu_2_5_0/bin/qemu-system-x86_64",""
#        "/opt/qemu/bin/qemu-system-x86_64",""
#        "-enable-kvm",""
#        "-S",""
#        "-bios","/opt/qemu/share/qemu/bios.bin",
        "-gdb", "tcp::%d,server,nowait" % (gdb_port),
        "-monitor", "tcp::%d,server,nowait,nodelay" % (monitor_port), 
       
        "-m", options.memory_size,
        "-smp", options.cpus,
        ]
#        "-drive", "file=%s,if=virtio,cache=%s" % (options.image_file, cache)]
    print args
    serial_port = int(options.serial_port)- 1 + int(options.vm_instance)
    if (options.networking_with_vhost):
# for netmap do insmod ./netmap_lin.ko 
#        args += ["-netdev","netmap,id=guest0,ifname=vale0.%s" %(options.vm_instance), "-device","virtio-net-pci,mac=00:30:48:DB:5E:0%s,netdev=guest0" % (options.vm_instance)]
        args += ["-netdev","tap,id=guest0,vhost=on,vhostforce", "-device","virtio-net-pci,mac=00:30:48:DB:5E:0%s,netdev=guest0" % (options.vm_instance)]
#    else:
#        args += ["-netdev","tap,id=guest0,vhost=off", "-device","virtio-net-pci,mac=00:30:48:DB:5E:0%s,netdev=guest0" % (options.vm_instance)]
 
    if (options.graphics):
        print "without graphics"
        if (options.with_p9):
            args += ["-fsdev","local,security_model=passthrough,id=fsdev0,path=/opt/jiny_root/","-device","virtio-9p-pci,id=fs0,fsdev=fsdev0,mount_tag=hostshare"]
        args += ["-vnc", ":%d" % (vnc_port),"-serial", "telnet::%d,server,nowait" % (serial_port)] 
        if (options.daemonize):
            args += ["-daemonize"]
    else:
        args += ["-nographic"]
        
    args += ["-append", "%s"%(options.kernel_args)]
    args += ["-kernel", "/opt_src/Jiny-Kernel/bin/jiny_image.bin"]
    args += ["-drive", "if=virtio,id=hdr0,file=/opt_src/Jiny-Kernel/bin/disk"]
    args += ["-drive", "if=none,id=drive1,file=/opt_src/Jiny-Kernel/bin/disk2"]
    args += ["-device", "pvscsi,id=vscsi0"]
    args += ["-device", "scsi-disk,bus=vscsi0.0,drive=drive1"]

    if (options.snapshot):
        args += ["-incoming", "exec: gzip -c -d /opt_src/Jiny-Kernel/bin/jiny_apic_snapshot.gz"]
#        args += ["-incoming", "exec: gzip -c -d /opt_src/Jiny-Kernel/util/s.gz"]

#    if(options.command):
#        start = open("../test/root/start","w");
#        start.write(options.command)
#        start.write("\nk shutdown\n");
#        start.close()
         
    try:
        # Save the current settings of the stty
        stty_save()
        print "starting the Qemu :" 
        print ["/usr/bin/sudo"] + args
        # Launch qemu
        qemu_env = os.environ.copy()
        subprocess.call(["/usr/bin/sudo"] + args , env = qemu_env)
        print "starting the telnet to serial port "
        if (options.daemonize):
            args = [
            "localhost", "%d" % (serial_port)]
            subprocess.call(["telnet"] + args, env = qemu_env)
    except OSError, e:
        if e.errno == errno.ENOENT:
          print("'qemu-system-x86_64' binary not found. Please install the qemu-system-x86 package.")
        else:
          print("OS error({0}): \"{1}\" while running qemu-system-x86_64 {2}".
                format(e.errno, e.strerror, " ".join(args)))
          
    stty_restore()
    
def main(options):
    start_jiny(options)

if (__name__ == "__main__"):
    parser = argparse.ArgumentParser(prog='run')
    parser.add_argument("-b", "--boot_without_grub", action="store_true",default=True,
                        help="boot without grub load the kernel directly")
    parser.add_argument("-c", "--cpus", action="store", default="1",
                        help="number of vcpus")
    parser.add_argument("-s", "--serial_port", action="store", default="50008",
                        help="serial port")
    parser.add_argument("-v", "--vm_instance", action="store", default="1",
                        help="vm instance")
    parser.add_argument("-m", "--memory_size", action="store", default="128M",
                        help="memory")
    parser.add_argument("-k", "--kernel_args", action="store", default="ipaddr=192.168.0.8 gw=192.168.0.1",
                        help="kernel arguments")
    parser.add_argument("-r", "--command", action="store", default="date",
                        help="command to run: example \"ls -l \"")
    
    parser.add_argument("-n", "--networking_without_vhost", action="store_true",default=False,
                        help="need networking without vhost")
    parser.add_argument("-N", "--networking_with_vhost", action="store_true",default=False,
                        help="need networking with vhost")
    parser.add_argument("-S", "--snapshot", action="store_true",
                        help="start from snapshot")
    parser.add_argument("-d", "--daemonize", action="store_true",default=True,
                        help="detach and login through serial")
    parser.add_argument("-p", "--with_p9", action="store_true",default=False,
                        help="withoutp9")
    parser.add_argument("-g", "--graphics", action="store_true", default=True,
                        help="start with graphics and user level file system ")
    cmdargs = parser.parse_args()
    main(cmdargs)    
    
