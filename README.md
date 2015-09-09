##JINY (Jana's tINY kernel)
[JINY](https://github.com/naredula-jana/Jiny-Kernel) was designed from ground up for superior performance on virtual machine.

1. **What is JINY?**.
 - Jiny is a Thin/Tiny unix like kernel with interface similar to linux, linux app can directly run on it without recompiling.
 - Designed from ground up: It is designed from the ground up to give superior performance on virtual machine(VM). The performance gain will come from reducing memory and cpu overhead when compare to traditional os like linux,freebsd etc.
 - High priority versus normal priority apps: The apps running on Jiny OS are divided in to high priority and normal priority. 
     - **High priority App**: Runs single app in ring-0, kernel cooperates with app to give maximum performance by reducing memory and cpu overhead. Any linux app can be converted in to high priority app by recompiling without any modification. Recompilation is required as the syscalls in libc need to be modified. When app is wrapped  by a  thin kernel like Jiny and launched as vm, it can give better performance even when compare to the same app on the metal. The two key reasons for the gain in performance is app will be running in a ring-0  with a co-operative thin kernel in a vcpu, and virtulization hardware will take the job in protecting the system from app malfunctioning.Currently Jiny can accommodate only one high priority app. The performance will be always high when compare to the similar app in the linux/freebsd vm. In apps that have high system call usage the performance will be better even when compare to the same app on the metal. JVM, memcached etc are well suitable to run as high priority app.  
     - **Normal priority app**: can run large in number like any traditional unix system in ring-3 with performance less when compare to high priority app.  

2. **How different is Jiny from OS like Linux?**
 - **Thin kernel** : Jiny is thin kernel, it is having very small set of drivers based on virtio, since it runs on hypervisor. Currently it supports only on x86_64 architecture. this makes the size of code small when compare to linux.
 - **OS for Cloud**: designed from groundup  to run on hypervisor, So it runs faster when compare to linux.
 - **Object oriented**: Most of subsystems will be in object oriented language like c++11.
 - **Single app inside the vm**: Designed to run single application efficiently when compare to traditional os.
 - **Network packet processing**: Most of cycles in packet processing is spent in the app context(i.e at the edge) as against in the bottom half in linux, this will minimizing the locks in the SMP. Detailed description is present in the [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf)
   
3. **For What purpose Jiny can be used?**
 - Running single apps like  JVM( tomcat or any java server), memcached  etc inside the Jiny vm as high priority app. Single app can be wrapped by a thin os like Jiny to enhance the performance.  Here the app will run much faster when compare to the same app in other vm's like freebsd or linux. Thin OS like JIny along with virtulization hardware can act like a performance enhancer for the apps on the metal.
 - Running multiple normal priority application like any traditional unix like systems with optimizations for vm. 


## Performance and Benchmarks Summary

More details of the Benchmarks are available at [Jiny-Benchmarks.](../master/doc/benchmarks.md).   

## Features currently Available:

- Page Cache:  LRU and MRU based (based on the published paper in opencirrus) 
- File Systems: 
   - TarFs : Tar file can be mounted as a root/non-root file system.
   - 9p 
   - Host based file system based on ivshm(Inter Vm Shared Memory) 
- Virtualization Features:
   - HighPriority Apps: very basic features is available.
   - Zero page optimization works along with KSM.
   - Elastic Cpu's: depending on the load, some of the cpu's will be rested.
   - Elastic Memory: depending on the load, some amount of physical memory can be disabled, so as host/other vm's can use.
- Virtualization drivers:
    - Xen : xenbus, network driver using xen paravirtualised.
    - KVM : virtio + P9 filesystem
    - KVM : virtio + Network (test server with udp stack(tcp/ip))
    - KVM : virtio + block
    - KVM : virtio + Memory ballooning
    - KVM : clock
- SMP: APIC,MSIX
- Networking:  Third party tcp/ip stacks as kernel module.
     - TCP/ip stack from uip ( from [AdamDunkels](https://github.com/adamdunkels/uip)  as kernel module. The above Benchamark-2 is with uip : currently only udp is supported, need to add tcp.
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


##Papers:
 -   [Page cache optimizations for Hadoop, published and presented in open cirrus-2011 summit](../master/doc/PageCache-Open-Cirrus.pdf) .
 -   [Memory optimization techniques](../master/doc/malloc_paper_techpulse_submit_final.pdf).
 -   [Jiny pagecache implementation](../master/doc/pagecache.txt)
 -   [Tar Fs - Jiny root file system](../master/doc/tar_fs.md)

##Related Projects:
 -[Vmstate](https://github.com/naredula-jana/vmstate): Virtualmachine state capture and analysis.