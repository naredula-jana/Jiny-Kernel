
##JINY (Jana's tINY kernel)
[JINY] is designed from ground up for superior performance on virtual machine.

1. What is JINY?.
 - Jiny is a  unix like kernel, linux app can directly run on it without recompiling because Jiny provides system call interface same or subset of linux. 
 - Designed from ground up: It is designed from the ground up to give superior performance on vm. The performance gain will come from reducing memory and cpu overhead when compare to traditional os like linux,freebsd etc.
 - High priority versus normal priority apps: The apps running on Jiny OS are divided in to high priority and normal priority. The high priority app like JVM,memcached can run faster when compare to the same app in linux vm. 
 - The number of high priority app's to run will be restricted 1 or 2 at any time because they will be running in kernel context with the same address space as that of user level app. Normal priority app's  will not be recompiled and not limited in number, the performance will be similar to that of Linux. High priority app need to recompile but change in code is not required.

2. What is the development plan and current status?.

  Phase-1: Developing traditional unix like kernel with small foot print(Completed)
 -  bringing kernel up on x86_64 without any user level app.
 -  Implementing most of the linux system calls, so that statically compiled app's on linux can run as it is. Currently app's like busybox can run as it is.
    
  phase-2: Changing kernel to run high priority app in kernel context(In Progress)
      - Changes in Jiny to support high priority app with superior performance when compare to linux vm.
      - to run the high priority app in the kernel context, this is to reduce the memory overhead. Tunning VM and task scheduler layer's to support high priority tasks.
      - to disable/minimize the interrupts on the cpu that is loaded with high priority app. this is reduce the cpu overhead and locks.
   b) converting most of subsystems from c to c++11.
     
3. For What purpose Jiny can be used?

 In the Past, it was used for:
  -  To model the page cache(MRU+LRU) suitable for hadoop like applications. 
  -  To model Host based filesystem(HFS): HFS is filesystem run in guest OS but does file i/o using the shared memory between the guest and host os. HFS does not need any block drivers, it communicated with block devices using the shared memory between guest and host OS.  
  - Benchmarking virtio net performance.
	
 In the future, it can be used run single high priority app in a vm:
   - Running specialized high priority app like  JVM(hadoop,or any java server), memcached  etc on the vm. 

##Benchmark:
A small c application reading and writing in to a /dev/null file repeatedly in a tight loop, this is to stress the syscalls. This app is executed on the metal and inside the Jiny as High priority and normal priority  and inside the llinux vm. The app executed very fast  

**Runtime of the app in different environment**:

- app as high priority in Jiny vm:      6 sec
- app as normal priority in Jiny vm: 55 sec
- same app on the metal(linux host): 44 sec
- same app on the linux vm:          43 sec

Reasons for High priority app superior performance when compare to the app on metal:


- **From cpu point of view**: when app is running in jiny vm, virtualization hardware is active and it will protect rest of resource  from malfunctioning the app.
             means if the app crashes it will not bring the host down, but only the vm crashes.
        when app is directly on the metal, virtualization hardware is passive/disabled and the os surrounding will make sure that the malfunctioning of the app will not bring down the host.
        so in the first case hardware is helping to protect from app malfunctioning, in the second case software(host os) will protect the app malfunctioning.
         Here OS protection means, app running in ring-3 and enters in to ring-0 using  syscall and there will copy of parameters and validations etc. All these is not needed in the first case. Since there is no such protection in Jiny High priority app, this will increase the efficiency of syscalls and context switches.
        
- **From end user point of view**: The same app need to recompile to run in  Jiny as high priority task. The app need to be launched though vm.
  Why do the recompilation needed: The app will be running the kernel context, so the syscall located in libc will replaced with corresponding function calls, The app will be running in ring-0 same as that of kernel.

 **Conclusion**: virtulization hardware can also be used to speedup the app.

## Feature Currently Available:

- Page Cache: - LRU and MRU based (based on the published paper in opencirrus) 
- File System: 
   - 9p+virtio
   - Host based file system based on ivshm(Inter Vm Shared Memory) 
- Virtualization Features:
   - HighPriority Apps: 
   - Elastic Cpu's: depending on the load, some of the cpu's will be rested.
   - Elastic Memory: depending on the load, some amount of physical memory can be disabled, so as host/other vm's can use.
- Virtualization drivers:
    - Xen : xenbus, network driver using xen paravirtualised.
    - KVM : virtio + P9 filesystem
    - KVM : virtio + Network (test server with udp stack(tcp/ip))
    - KVM : virtio + memory ballooning
    - KVM : clock
- SMP: APIC,MSIX
- Networking:  LWIP4.0 as a kernel module
- Debug features:
   - memoryleak detection.
   - function tracing or call graph.
   - syscall debugging.
   - stack capture at any point. 
- Loadable Modules:
   - Supports loadable kernel module. Lwip tcp/ip stack compiled as kernel module.
- User level:
   - Statically compiled user app can be launched from kernel shell or busy box.
   - busybox shell can successfully run on top of Jiny kernel, network apps can able to use socket layer.
- Hardware:
  It was fully tested for x86/64. partially done for x86_32.

##TODO

 -  Supporting High priority app fully: currently it is loaded as a kernel module.
 -  Reducing start up time.
 


##Papers:
 -   [page cache optimizations for Hadoop appered @ open cirrus-2011](../master/doc/PageCache-Open-Cirrus.pdf) .
 -   [memory optimization techniques](../master/doc/malloc_paper_techpulse_submit_final.pdf).
 -   [jiny pagecache implementation](../master/doc/pagecache.txt)