

**Procedure to Run the Image from master branch:**

Below Docker Container will execute the default image from master branch. 

```
  docker run --privileged --rm -it --entrypoint "/run/master_run"  naredulajana/jiny_base:latest

```

**Procedure to Build the Image from local git repo source and run:**

 
Execute the below docker script from the top directory. Docker Container will pick the source code from  local host, after that it build the image and then execute the image.

``` 
 ./docker_compile_run
 
 with Optional configurations: 
 
  ./docker_compile_run -smp 2    /* by default only one cpu core, run the jiny kernel with 2 or more cores */
  ./docker_compile_run -enable-kvm /* use kvm hypervisor, this option will make kernel to run lot faster due to hardware accelaration from vt-x */
  ./docker_compile_run -gdb tcp::1336 /* with this option gdb can be attach to debug the kernel */
  ./docker_compile_run  -netdev tap,id=guest0 -device virtio-net-pci,mac=00:30:48:DB:5E:09,netdev=guest0   /* with nic card */
 

```



** Useful calls:**


Below are commands used to peak the kernel:

```

CPU related:
-----------
k cpu  : cpu cores and interrupts  :  similar to cat /proc/interrupts
k ps : list of threads or process: similar "ps -ax"
k ps all : list of threads and corresponding stacks. very very usful for debugging


Memory Related:
---------------
k mem  : physical memory utilization 
k jslab/jslabmalloc : memory slab
k pc : page cache
k pt : page table for any thread
k maps <pid> : memory map for any thread
k balloon : memory mallon driver

VFS related:
-------------
k ls : vfs list of files.
k mount: list of mounts


Network related:
-----------------
k ifconfig : network info
k arp_stat : arp table
k network : list of sockets
k net_stat  : network stats


MISC:
-------
k jdevices : list of pci devices detected during bootup, it also list the drivers attached to devices.
k lsmod/rmmod/insmod : commands related to modules 


DEBUG :
--------
k dmesg :  log 
k call_stats :  call stats
k locks : list of locks and stats.
k obj_list : list of c++ objects types and instants


```
