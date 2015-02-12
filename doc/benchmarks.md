## Optimizations and Performance improvements .

This paper provides performance comparision of [Jiny Kernel](https://github.com/naredula-jana/Jiny-Kernel)   with  Linux on different workloads like cpu,network,storage,memory etc:
In the below benchmarks, Jiny as performed better then linux in the below specific workloads, one of the reason is linux is  generic kernel designed to run on the metal  when compare to Jiny. It is relatively easy to improve the througput for the specific workloads instead of generic. 
  
- Benchmark-1(CPU centric): Comparisions of cpu centric app running in linux vm versus same app running in Jiny vm. There is big improvement when the same app run in Jiny vm as High priority app. 
- Benchmark-2(Network centric): Comparisions of network throughput in linux vm versus Jiny vm. There is 20%-50% improvement in network throughput in Jiny Vm when compare to linux vm on the same hardware.
- Benchmark-3(Storage centric): In progress.
- Benchmark-4(PageCache): Comparisions of Read/write throughput for Hadoop workload. Improvement of 20% in read/write throughput of hdfs/hadoop workloads.
- Benchmark-5(Malloc): Memory  improvements with zero page accumulation and other related techiniques: In progress

----------------------------------------------------------------------------------

###Benchmark-1(CPU centric): Completed
**Overview:** An application execution time on the metal is compared with the same app wrapped using thin OS like Jiny and launched as vm. The single app running in Jiny vm outperforms by completing in 6 seconds when compare to the same running on the metal that took 44sec. The key reason for superior performance in Jiny os is, it accelerates the app by allowing  it to run in ring-0 so that the overhead of system calls and context switches are minimized. The protection of system from app malfunctioning is left to virtulization hardware, means if the app crashes in ring-0 at the maximum vm goes down, but not the host. To run in Jiny vm, the app need to recompile without any changes using the modified c library.

**Application(app) used for testing:** A simple C application that read and writes in to a /dev/null file repeatedly in a tight loop is used, this is a system call intensive application. [The app](https://github.com/naredula-jana/Jiny-Kernel/blob/master/modules/test_file/test_file.c) is executed in four environments on the metal, inside the Jiny as High priority, as normal priority inside Jiny  and inside the linux vm.   

#####Completion time of app in different environments:

1. **case-1:** app as high priority inside Jiny vm:      6 sec
2. **case-2:** same app on the metal(linux host): 44 sec
3. **case-3:** same app as normal priority in Jiny vm: 55 sec
4. **case-4:** same app on the linux vm:          43 sec

#####Reasons for superior performance:

- **From cpu point of view**: when app runs inside jiny vm(case-1), virtualization hardware(like intel VT-x) is active and it will protect system  from app malfunctioning, here app runs in ring-0 along with Jiny kernel. means if the app crashes it will not bring down the host, but only the vm crashes at the maximum. when app on the metal(case-2) , virtualization hardware is passive/disabled and the os surrounding(i.e host os) will make sure that the app malfunctioning will not bring down the host by running the app in ring-3. 

 One of key difference between case-1 and case-2 is, in case-1 vt-x hardware is used while in case-2 host os software does the job of system protection from app malfunctioning. Jiny OS will allow the app to run in the same address space, it does not spend cpu cycles to protect the system or os from app malfunciton, at the maximum the vm goes down since only one app is running it is equal to single app crashing without effecting the system.

        
- **From end user point of view**: To run the app inside the Jiny vm as high priority app, it need to recompile so the syscall located in libc will replaced with corresponding function calls. app is not required any change , only libc will be modified. start time in the first case will be slightly slower since vm need to be started before the app. so long running applications like servers will be suitable for high priority app.

##### summary
 1. Virtulization hardware(vt-x) along with thin OS like Jiny can function as hardware assit  layer to speedup the app. Launching single app using Jiny Vm will be useful not only from virtulization point of view but also to increase the speed.
 2. In the test,I have used syscall intensive app that as shown huge improvement when compare to app on metal, but other workload like io intensive may not give that large improvement.  Speeds of virtulization io path are improving continuously both in software or hardware,  so  io intensive  apps also will become better in the future.
 3. Most of apps as  high priority app in Jiny will  show big performance improvement when compare the same app in linux or freebsd vm's. 

----------------------------------------------------------------------------------

###Benchmark-2(Network centric): Completed
This benchmark concentrates on the networking speed between linux and Jiny OS. Networking in Jiny is based on the  [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). Udp client and server are used to test the maxumum throughput of the networking stack in the os. udp client on the host sends the packet to the udp server inside the vm, udp server responds back the packet, In this way the amount of bytes/packets processes in the vm will be calculated. The below test results shows the network thoughput(in terms of bytes processed) in Jiny  and linux:

**Test Environment**:  
**Network Driver** : Virtio+vhost.  
**Packet size** used by udp client: 200 bytes. 
Number of cpu cores in the linux and Jiny Vm are 2.  
**Hypervisor**: kvm/qemu-2.x

vm to vm: on a low end Hardware

1. **Test-1**: ubuntu-14(linux vm) :  able to transfer 94 Mbps between two linux vm's.
2. **Test-2**: Jiny os : able to transfer 155 Mbps between two jiny vm's.
3. **Test-3**: Jiny os with delay in send door bell : able to transfer 170 Mbps between two jiny vm's. 

Host to vm: on a highend hardware.

4. **Test-4**:  udp_client on linux host and udp_server on linux vm : 180Mbps.
5. **Test-5**:  udp_client on linux host and udp_server on jiny vm : 260Mbps.


##### summary of results
 1. Difference between Test-1 and Test-2: Processing the packet in Linux and Jiny are completely different. In Jiny , most of the cpu cycles are spend in the application context as mentioned in  [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). whereas in linux, cpu cycles are split between the app and Network bottom half making packet to process by different cores. This may be one of the reason why Jiny performance better then linux.
 2. Difference between Test-2(155M) and Test-3(170M):  For every packet send on the NIC, issuing the door bell in virtio driver cost extra MMIO operation, that is causing the vm exits in kvm hypervisor, this was the reason test-3 got some 15Mbytes extra processing. postponing doorbell for few packets/for a duration of time as improved the throughput at load, but this cause extra delay in holding the send packet when the system is under load. This can be turned on/off depending on the load just like imterrupts.
 3. Test-4 and Test-5 :  Here udp_client is resource intensive when compare to udp_server. In this tests, vm is mainly used for network thoughput. Here Jiny as performed better by 30% when compare to linux. Test-4 and 5 uses powerful cpu when compare to Test1-3 

 
 
##### Reasons for Better throuhput in Jiny
1. network bottom half is very thin , and entire computation of send/recv packet is done in the application context, this makes packet processing always happens on the same core and avoid locking. The implementaion of network frame work is similar to [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). 
2. lockless implementation of memory management in acquiring and releasing the memory buffers.
3. Minimising the virtio kicks in send path and interrupts in recv path.
4. **Area to Improve**: a) checksum computation takes large amount of cpu cycles, need to improve further in this area. b) avoiding/minimizing the spin locks.
5. Making zero copy: show 3% improvement by avoiding one copy from  user to kernel and viceversa, but the improvement is not big
 
 

---------------------------------------------------------------------------------- 
###Benchmark-3(Storage centric): In Progress:
  currently jiny uses  [tar file sytem](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/tar_fs.md) with virtio-disk without vhost, once vhost is available then it can be tuned and benchmark against linux similer to the bencmark-2. 

----------------------------------------------------------------------------------
###Benchmark-4(PageCache): Completed.
   Details available in this paper ["Page cache optimizations for Hadoop".](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/PageCache-Open-Cirrus.pdf).  This paper was presented in opencirrus 2011 summit. Jiny uses similar page algorithm as mentioned in the paper.
   
----------------------------------------------------------------------------------
###Benchmark-5(Malloc): In Progress
  Memory allocation improvements like zero page accumulation and other related techiniques: Based on this technique one round of testing is done but it as not given the substantial improvements as expected, need to improve and redo.
   
----------------------------------------------------------------------------------
###Benchmark-6(Lock free or avoiding spinlocks  and minimizing the interrupts): In Progress
  - spin locks are well suited for SMP not for virtual machines because of lock-holder preemption. Avoiding spin locks and writing the lock free code.
  - Avoid the following: hitting "hlt" instruction by a CPU and getting waken up by other cpu using IPI(inter processor insr) interrupt within short period of time is not good for virtual machines.
  - Minimising the regular timer Ticks for every 10ms.
   
----------------------------------------------------------------------------------
##Papers:
 -   [Page cache optimizations for Hadoop, published and presented in open cirrus-2011 summit](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/PageCache-Open-Cirrus.pdf) .
 -   [Memory optimization techniques](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/malloc_paper_techpulse_submit_final.pdf).
 -   [Jiny pagecache implementation](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/pagecache.txt)
 -   [Tar Fs - Jiny root file system](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/tar_fs.md)

##Related Projects:
 -   [Jiny Kernel](https://github.com/naredula-jana/Jiny-Kernel) .
 -   [Vmstate](https://github.com/naredula-jana/vmstate): Virtualmachine state capture and analysis.
 