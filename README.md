## JINY (Jana's tINY kernel)
[JINY](https://github.com/naredula-jana/Jiny-Kernel) is a cloud OS designed from ground up for superior performance on virtual machine.

1. **What is JINY?**.
 - Unlike traditional kernel Jiny is thin kernel designed for cloud. Jiny provides posix interface similar to linux, so linux application can directly run on it without recompiling.
 - Designed from ground up: It is cloud OS designed from the ground up to give superior performance on virtual machine(VM). The performance gain comes from reducing isolation, reducing protection, increasing  memory and cpu efficiency. 
 - High Priority(HP) and normal priority mode: Jiny OS runs in two modes. 
     - **High priority mode or Single App mode**: In this mode, kernel functions like library OS and does the supporting role for the app, designed with low overhead. Only one application can be launched in the OS, and application runs in the same ring as that of OS(i.e ring-0). The functions which are considered “system calls” on Linux( e.g., futex,read ,..etc)  are ordinary function calls in Jiny and do not incur syscall call overheads, nor do they incur the cost of user to kernel parameter copying which is unnecessary in  single-application OS. fork and exec posix calls are not supported in this mode. App is launched in a different way. Apart from posix calls, Non posix API’s between application and Guest OS will further decreases latency and improves throughput.  Currently golang and C  application are supported in this mode.  
     - **Normal priority mode**: Jiny kernel emulates most of the posix calls(system calls) compatible to linux, so that linux binaries can run as it is. Multiple applications can run in this mode at the same time similar to traditional OS.  

2. **How different is Jiny from OS like Linux?**
 - **Thin kernel** : Jiny is thin kernel, it is having very small set of drivers based on virtio, since it runs on hypervisor. Currently it supports only x86_64 architecture, this makes the size of code small when compare to linux.
 - **OS for Cloud**: Designed from ground-up  to run on hypervisor, So it runs faster when compare to linux.
 - **Object oriented**: Most of subsystems are in object oriented language c++11.
 - **Non-posix api's for jvm and golang runtime system**: Supports special api's for jvm and golang runtime system for running app in HP mode. Here java and golang app does not need any change.
 - **Single app inside the vm**: Designed to run single application efficiently when compare to traditional os.
 - **Network packet processing**: Most of cycles in packet processing is spent in the app context(i.e at the edge) as against in the bottom half in linux, this will minimizing the locks in the SMP. Detailed description is present in the [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf)
   
3. **For What purpose Jiny can be used?**
 - Running single apps like  JVM( any java server), golang apps, memcached  etc inside the Jiny vm in high priority mode. Here the app will run much faster when compare to the same app in other vm's like freebsd or linux. Thin OS like Jiny along with virtulization hardware can act like a performance enhancer for the apps on the metal.
 - Running multiple normal priority application like any traditional unix like systems with optimizations for vm. 


## Procedure to  compile and run:
 
More details of [compiling and running Jiny Kernel is available here.](../master/bin/README.md).  

## Performance and Benchmarks:

More details of the Benchmarks are available at [Jiny-Benchmarks.](../master/doc/benchmarks.md).   

## Features currently Available:

- Page Cache:  LRU and MRU based (based on the published paper in opencirrus for Hadoop) 
- File Systems: 
   - TarFs : Tar file can be mounted as a root/non-root file system.
   - 9p 
   - Host based file system based on IVSHM(Inter Vm Shared Memory) 
- Virtualization Features:
   - HighPriority Apps: very basic features is available(app to load as module).
   - Zero page optimization works along with KSM.
   - Elastic Cpu's: depending on the load, some of the cpu's will be rested.
   - Elastic Memory: depending on the load, some amount of physical memory can be disabled, so as host/other vm's can use.
- Virtualization drivers:
    - Xen : xenbus, network driver using xen paravirtualised.
    - KVM : virtio + P9 filesystem
    - KVM : virtio + Network (vhost-net,vhost-user), with multi-queue
    - KVM : virtio + block (vitio-disk) with multi-queue
    - KVM : virtio + Memory ballooning
    - KVM : clock
    - Vmware : vmxnet3,pvscsi
- SMP: APIC,MSIX
- Networking:  Third party tcp/ip stacks as kernel module.
     - TCP/ip stack from UIP ( from [AdamDunkels](https://github.com/adamdunkels/uip)  as kernel module. The above Benchamark-2 is with uip : currently only udp is supported, need to add tcp.
     - LWIP4.0 as a kernel module: 
- Debugging features:
   - memoryleak detection.
   - function tracing or call graph.
   - syscall debugging.
   - stack capture at any point. 
   - code profiling. 
- Loadable Modules:  Supports loadable kernel module. Lwip tcp/ip stack compiled as kernel module.
- User level:
   - Statically and dynamically compiled user app can be launched from kernel shell or busy box.
   - busybox shell can successfully run on top of Jiny kernel, network apps can able to use socket layer.
- Hardware: It was fully tested for x86/64. partially done for x86_32.
- High Priorty mode: support c apps and golang applications. golang appliction does not need any change. [changes are needed to golang runtime system](../master/modules/HP_golang_changes).


## Papers:
 -   [Page cache optimizations for Hadoop/HDFS, published and presented in open cirrus-2011 summit](../master/doc/PageCache-Open-Cirrus.pdf) .
 -   [User space Memory optimization techniques](../master/doc/malloc_paper_techpulse_submit_final.pdf).
 -   [Jiny pagecache implementation](../master/doc/pagecache.txt).
 -   [Tar Fs - Jiny root file system](../master/doc/tar_fs.md).
 -   [Jiny Kernel Memory optimizations](../master/doc/Jiny_memory_management.md).
 -   [Golang apps in ring-0](../master/doc/GolangAppInRing0.pdf).
 -   [Perf tool to measure the speed of vm/app](../master/doc/Perf_IPC.pdf).

## Related Projects:
 -[Vmstate](https://github.com/naredula-jana/vmstate): Virtualmachine state capture and analysis.
