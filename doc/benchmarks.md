## Optimizations and Performance improvements .

[Jiny kernel](https://github.com/naredula-jana/Jiny-Kernel) is designed from ground up for virtual environment. This paper provides performance comparision [Jiny Kernel](https://github.com/naredula-jana/Jiny-Kernel)   with  Linux on different workloads like cpu,network,storage,memory,IPC etc on the virtual platform  like kvm/qemu.
The below benchmarks show Jiny as performed better then linux, one of the reason is linux is generic kernel designed to run on the metal  when compare to Jiny designed for virtual platform. The following are performance enhancments and techniques used in different work loads:
  
- Benchmark-1(CPU centric): Comparisions of cpu centric app running in linux vm versus same app running in Jiny vm. There is big improvement when the same app run in Jiny vm as High priority app. 
- Benchmark-2.1(Network centric - virtio-vhost): Comparisions of network throughput in linux vm versus Jiny vm. There is 20%-50% improvement in network throughput in Jiny Vm when compare to linux vm on the same hardware.
- Benchmark-2.2(Network centric - Virtio-user): Similar to the above(2.1), except the switch will located at the userspace instead of host kernel(linux bridge/ovs-switch).
- Benchmark-3(Storage centric): In progress.
- Benchmark-4(PageCache): Comparisions of Read/write throughput for Hadoop workload. Improvement of 20% in read/write throughput of hdfs/hadoop workloads.
- Benchmark-5(Malloc): Memory  improvements with zero page accumulation and other related techiniques: In progress
- Benchmark-6(IPC, locks and interrupts): locks and interrutpts improvement: more then 100% improvement : Partially completed.

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

###Benchmark-2.1(Network centric - virtio-vhost): Completed
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

  
##### Reasons for Better throughput in Jiny
1. network bottom half is very thin , and entire computation of send/recv packet is done in the application context, this makes packet processing always happens on the same core and avoid locking. The implementaion of network frame work is similar to [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). 
2. lockless implementation of memory management in acquiring and releasing the memory buffers.
3. Minimising the virtio kicks in send path and interrupts in recv path.
4. **Area to Improve**: a) checksum computation takes large amount of cpu cycles, need to improve further in this area. b) avoiding/minimizing the spin locks.
5. Making zero copy: show 3% improvement by avoiding one copy from  user to kernel and viceversa, but the improvement is not big.

---------------------------------------------------------------------------------- 
 
###Benchmark-2.2(Network centric - virtio-user): In Progress:

   In the above Benchmark(2.1), switch is linux bridge located in host kernel. Performance can be improved by replacing the linux bridge(virtio-vhost) with the userspace bridge using qemu based vhost-user vnics, here there will not be any change from guest os, but some of the tunning may help(like multiqueue). The packets from one vm to another vm goes through the shared memory in the user space instead of switching the packet through the linux host kernel bridge/ovs. If the ethernet switch located at the userspace, it can save the following:
   
   - instead of two threads(kernel vhost threads) and linux bridge to handle the packet, it can be done by a single user levelthread.
   - one copy from virtio ring can be saved, it can be done by copying the packet from vitio ring to another directly.
   - the user space switch can be scaled-in/out easily when compare to linux bridge.
      
   user level switches like snabb ethernet switch, netmap(currently does not have vhost-user interface) or similar switch can be used to acheive the above. userlevel switch are well suited if the vm's on the same host need to communicate like NFV etc.
      
   Performance bechmarks between linux-bridge vs user level switch , and between jiny vs linux : TODO
   
   1. **Test-1**: between two jiny vm's with user based switch using virtio-user :  able to transfer and recv 300 Mbps(bi-directional) with 200bytes packet size.
   2. **Test-2**: between two linux vm's with linux bridge using virtio-vhost :  able to transfer and recv 148 Mbps(bi-directional) with 200bytes packet size.
   
   Further Improvements in userspace bridge:
   1. bridge and vm pinning to a particular cpu with NUMA friendly. this can help in cpu cache and gives better throughput.
   2. virtio with multiple queues, this need some changes in qemu, currently virtio-user does not supports, increasing the threads in user level switch according to the queues.

source code for  [user level virtio switch](https://github.com/naredula-jana/Jiny-Kernel/tree/master/test/virtio-switch). This is  2 port hub, does not interpret the packet, when ever packet arrives on a port, the packet will be copied to another port. This code was based on refernce implementation of virtio-user present @ https://github.com/virtualopensystems/vapp.git.

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
###Benchmark-6(Speeding IPC): Partially completed
This Benchmark main focus on performance gaps in InterProcess Communication(mutex,semphore,messages etc).The gaps are mainly from the angle of locks and interrupts(especially IPI and timer interrupts) inside the kernel. 

  - spin locks are well suited for SMP not for virtual machines because of lock-holder preemption. Avoiding spin locks and writing the lock free code.
  -  Hitting "hlt" instruction by a CPU and getting awaken up by other cpu using IPI(Inter Processor Interrupt) within short period of time is not good for virtual machines.
  - Minimising/Avoiding the regular timer Ticks used in many kernels.
  
   **Test Environment and Results**: 
     This is a Producer and consumer test using semaphores. Producer and consumer runs in two seperate threads. Producer is more cpu intesive when compare to consumer, this make consumer waits for producer at the end of each loop. producer wakes consumer once item is produced. In this way consumer get context switched at the end of every loop. This emulates  producer and consumer with unequal cpu computation to process every item, this will be a common case. The [source code for test available here.] (https://github.com/naredula-jana/Jiny-Kernel/blob/master/test/expirements/sem_test4.c). If the amount of computation for producer and consumer is same then there will be not be any lock contention then it will run at full speed without entreing in to kernel. This test emulates the non-good case.
     **Hypervisor**: kvm/qemu-2.x
     **host kernel version**:3.13.0-32
     **command and arguments to run the test**:  "sem 800k 2000"  :  Here 800k represents the number of items produced and consumed, producer loops 2000 times in tight loop to emulate cpu consumption after every item, the more this value the more will be the consumer wait time.
     
   1. **Test-1**: with ubuntu-14(linux vm - kernel 3.0.x) on 2 cpu :  able to complete the IPC test in 22 seconds. host cpu consumption is 150%(1.5 cpu's) noticed on host with top utility. large number of IPI interrupts close to 280k are generated inside the guestos.
   2. **Test-2**: Jiny os with 2 cpu as vm(version 2.x): able to complete in 9 seconds(more then 140% improvement). host cpu consumption is 200%(2 cpu's) noticed with top utility. No additional IPI interrupts are generated in the quest OS.
   3. **Test-3**: ubuntu-14(linux os) on metal: almost same as Test-2(takes 9 sec), with large loops metal takes 97sec whereas Test-2 takes 93 seconds, means on the metal it takes almost 3% to 4% more, this may be because of IPI interrupts on the metal.
   4. **Test-4**: with ubuntu-14(linux vm - kernel 3.0.x) on 2 cpu : same as Test-1 with "cpu hogger" program in background with lowest priority(using nice command). "cpu hogger" is a simple c program in a infinite loop with 2 threads, this program started at priority 20(lowest priority) to avoid the cpu entering into idle state. In this case, same IPC test as in test-1 completes in 12 seconds. So the IPC test as improved from 22sec to 12sec(regained 110% out of 140%) with cpu-hogger in background. 
          
##### Reasons for Better throughput of IPC in Jiny OS
1. Producer and consumer thread will be running on seperate cpu's. when consumer thread goes to sleep and control goes to idle thread, before hitting the "hlt" instruction idle thread checks if there is any threads belonging to the cpu is waiting, if there is any threads under wait then it spin the cpus or does a small house keeping tasks for a short duration(10ms or 1 tick) then it goes to sleep using "hlt" instruction, In this short duration if the consumer thread got woken up then it saves IPI(interrupts) generation and exit/entry of the vcpu. In this way it saves around 800k interrupts for 800k loop in Test-2. Interrupts are costly in virtual environment when compare to metal. every interrupt causes exist and entry in the hypervisor. The "hlt" instruction casues context switch at host.        
2. switching off the timer interrupts on the non-boot cpu (not tested or not included) MAY save somemore cpu cycles and can speedup further.

##### Summary of Tests:
1. Test-1 versus Teset-2 : Jiny-vm as performed 140% better when compare to linux vm with same test setup, this is mainly interrupts are not generated because cpu avoids hitting "hlt" instruction when the thread is under wait for a short period.
2. Test-2 versus Test-3 :linux on baremetal is very close to that of jiny vm. 3% to 4% degredation is due to the IPI interrupts are generated on the metal when compare to jiny.
3. Test-1 versus Test-3 :linux kernel on the metal and on hypervisor(i,e test-1 and test-3) shows a gap of 140%. this is mainly the interrupts speed on hypervisor and hitting hlt instruction frequently. whenever "hlt" instruction is execute din guest os, control goes to kvm and get back to guest os upon wakeup, this transfer is costly when compare to small cpu spin in guest os. This is exactly implmented in jiny os to avoid hitting the "hlt" instruction. 

Issue-1: In IPC, if there is a contention in mutext or semop or other IPC, then one of the thread will be active and other thread goes to sleep. In this situation, It will be costly to wakeup a thread under sleep in virtual environment with linux  when compare to the metal or jinyos. 

##### When IPC based apps in linux vm can under perform due to Issue-1
1. linux as vm and with multicore(more then one cpu). The issue will not be present in single core or on metal(present but very minumum as shown it is only 3 to 4% as against 140%).
2. The issue will be more when the cpu's are idle and only one task is on the cpu. example: only producer,consumer threads running on different cpu's.  If other app's are running/active on the same vcpu as waiting thread then vcpu will not go to sleep and issue-1 will not popup.
3. The issue will popup if the critical section's in the app's are small and there is contention among the threads. If the critical section are large then waiter thread goes to a long sleep, in this case receving IPI interrupts is better when compare to tight cpu spin.

 Monitoring Issue-1: Watch out  the number of [Rescheduling Interrupts](https://help.ubuntu.com/community/ReschedulingInterrupts) in /proc/interrupts, Rescheduling interrupts are implemented using Inter Processor Interrupt(IPI). 

----------------------------------------------------------------------------------
##Papers:
 -   [Page cache optimizations for Hadoop, published and presented in open cirrus-2011 summit](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/PageCache-Open-Cirrus.pdf) .
 -   [Memory optimization techniques](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/malloc_paper_techpulse_submit_final.pdf).
 -   [Jiny pagecache implementation](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/pagecache.txt)
 -   [Tar Fs - Jiny root file system](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/tar_fs.md)

##Related Projects:
 -   [Jiny Kernel](https://github.com/naredula-jana/Jiny-Kernel) .
 -   [Vmstate](https://github.com/naredula-jana/vmstate): Virtualmachine state capture and analysis.
 