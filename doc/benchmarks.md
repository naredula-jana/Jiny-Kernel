## Benchmarks and Performance improvements in Opensource Virtual Infra.

[Jiny kernel](https://github.com/naredula-jana/Jiny-Kernel) was designed from ground up for superior performance on virtual infrastructure, so as vm can run to near native performance. This paper mainly compares throughput of [Jiny Kernel](https://github.com/naredula-jana/Jiny-Kernel) with Linux kernel on different workloads like cpu,network,storage,memory,IPC,.. etc, with hypervisors like kvm. Apart from kernel,other key virtual infra components like virtual switch throughput are also compared.
In each benchmark the bottlenecks and corresponding optimizations were highlighted.  This paper was limited only to the opensource virtual infrastructure software components like Jiny,Linux, kvm, qemu, virtual switches(linux bridge,userspace switch,ovs,snabb),DPDK..etc.  There are lot of performance centric areas like storage,memory.. etc that are yet to be explored.

**About me:**
 I have started developing Jiny Kernel almost 5 years back from scratch, Along with Jiny development, these Benchmarks are developed over the period of time. The paper highlights performance improvements in areas like pagecache suitable for Hadoop workloads, IPC improvements for multi threaded apps, Network throughput improvements for inter vm communication within same host suitable for NFV,..etc. 

   <table border="1" style="width:100%">
  <tr>
    <td><b> -  </b></td>
    <td><b> Area </b></td>
    <td><b>Description </b></td>
    <td><b> Improvements</b></td>
    </tr>
    <tr><td>Benchmark-1</td><td>CPU centric</td> <td> Comparisions of cpu centric app running in linux vm versus same app running in Jiny vm.</td> <td> 7X </td></tr>
    <tr><td>Benchmark-2</td><td>Network centric</td> <td>Comparisions of network throughput in linux vm versus Jiny vm as against with linux bridge and user space switch. Jiny on user space switch outperforms when compare to the kernel based switch like linux bridge.</td> <td> 7X </td></tr>
     <tr><td>Benchmark-3</td><td>Storage</td> <td> Incomplete</td> <td> - </td></tr>
     <tr><td>Benchmark-4</td><td>PageCache</td> <td> Comparisions of Read/write throughput for Hadoop workload, especially hdfs or IO centric. </td> <td> 20% </td></tr>
     <tr><td>Benchmark-5</td><td>Memory</td> <td> Memory  improvements with zero page accumulation and other related techiniques</td> <td> - </td></tr>
      <tr><td>Benchmark-6</td><td>IPC</td> <td> IPC and message passing.</td> <td> 140% </td></tr>
    </table>
    
----------------------------------------------------------------------------------

###Benchmark-1(CPU centric): 
**Overview:** An application execution time on the metal is compared with the same app wrapped using thin OS like Jiny and launched as vm. The single app running in Jiny vm outperforms by completing in 6 seconds when compare to the same running on the metal that took 44sec. The key reason for superior performance in Jiny os is, it accelerates the app by allowing  it to run in ring-0 so that the overhead of system calls and context switches are minimized. The protection of system from app malfunctioning is left to virtulization hardware, means if the app crashes in ring-0 at the maximum vm goes down, but not the host. To run in Jiny vm, the app need to recompile using the modified c library. libc need to modify to convert the system calls in to plain function calls.

**Application(app) used for testing:** A simple C application that read and writes into a /dev/null file repeatedly in a tight loop is used, the app is a system call intensive. [The app](https://github.com/naredula-jana/Jiny-Kernel/blob/master/modules/test_file/test_file.c) is executed in four environments on the metal, inside the Jiny as High priority, as normal priority inside Jiny  and inside the linux vm.   

#####Completion time of app in different environments:

1. **case-1:** app as high priority inside Jiny vm:      6 sec
2. **case-2:** same app on the metal(linux host): 44 sec
3. **case-3:** same app as normal priority in Jiny vm: 55 sec
4. **case-4:** same app on the linux vm:          43 sec

#####Reasons for better performance:

- **From cpu point of view**: when app runs inside jiny vm(case-1), virtualization hardware(like intel VT-x) is active and it will protect system  from app malfunctioning, here app runs in ring-0 along with Jiny kernel. means if the app crashes it will not bring down the host, but only the vm crashes at the maximum. when app on the metal(case-2) , virtualization hardware is passive/disabled and the os surrounding(i.e host os) will make sure that the app malfunctioning will not bring down the host by running the app in ring-3. 

 One key difference between case-1 and case-2 is, in case-1 vt-x hardware is used while in case-2 host os software does the job of system protection from app malfunctioning. Jiny OS will allow the app to run in the same address space, it does not spend cpu cycles to protect the system or os from app malfunciton, at the maximum the vm goes down since only one app is running it is equal to single app crashing without effecting the system.

        
- **From end user point of view**: To run the app inside the Jiny vm as high priority app, it need to recompile so the syscall located in libc will replaced with corresponding function calls. app is not required any change , only libc will be modified. start time in the first case will be slightly slower since vm need to be started before the app. so long running applications like servers will be suitable for high priority app.

##### summary
 1. Virtulization hardware(vt-x) along with thin OS like Jiny can function as hardware assit layer to speedup the app. Launching single app using Jiny Vm will be useful not only from virtulization point of view but also to increase the speed.
 2. In the test, syscall intensive app was used, and  as shown huge improvement when compare to app on metal, but other workload like io intensive will not give that large improvement.  Speeds of virtulization io path are improving continuously both in software or hardware,  so  io intensive  apps also will become better. 

----------------------------------------------------------------------------------

###Benchmark-2(Network centric): 

This benchmark measures the network throughput between two vm's using virtual switch on the same host. This mainly highlight the bottlenecks in quest kernel or virtual switch connecting the vm's. Networking in Jiny is based on the [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). This benchmark gives the maxumum throughput in terms of packets per second(PPS) at which first vm can send udp packets to second vm using the virtual switch in between. 

 - **Test Environment**:  In this benchmark, udp client and udp server socket applications are used to send and recv the udp packets from one vm to another. udp server acts like a reflector, responds back the same packet recvied from udp client. udp client counts the number of packet it got responded back from the udp server. udp client is executed in the first vm and udp server is executed in the second vm. The below test results shows the network thoughput(in terms of million packets processed per second(MPPS) on sending/recving side.The  tests mentioned in the table compares Jiny and Linux kernel througput using one of the two virtual switches with various configurations.  
 - **kernels** : Jiny kernel(2.x) , linux kernel(3.18) are used. 
 - **virtual switch**: Linux Bridge(LB), [User Space Switch(USS)](https://github.com/naredula-jana/Jiny-Kernel/tree/master/test/virtio-switch): These two vswitches are compared against the above two kernels. USS is a simple two port virtual user space switch to connect two vm's. It is based on opensource vhost-user vNIC, this vNIC is available in kvm hypervisor. other User space virtual switch are Snabb,DPDK,..etc . 
 - **VNIC type**: uses vhost-kernel or virtio-vhost with LB, uses vhost-user with USS bypassing host kernel.
 - **Packet size** used by udp client/server: 50/200 bytes. 
 - **Number of cpu cores**:  vm with 2 to 6 cores are used, udp client consumes 1 to 4 thread/cores to generate and recv such large number of packets.  
 - **Hypervisor**: kvm/qemu-2.4/linux host-kernel-3.18.
 - **two way versus one way test**: "two way" means udp client sends the packets to the udp server running on other vm, where udp_server recves the packets it response back the same packet to the udp client. In the one way, udp server will not be running on the destination vm,so the packets will be dropped in the recving side of the OS or in the switch. 

 <table border="1" style="width:100%">
  <tr>
    <td><b>Test# </b></td>
    <td><b> Description</b></td>
    <td><b>Using Linux kernel inside both vm's</b></td>
    <td><b>Using Jiny kernel inside both vm's</b></td>
    </tr>
  <tr>
    <td> 1 </td>
    <td> vm1 -LB- vm2 </td>
    <td>Throughput for two way: 0.120 MPPS<br>Bottleneck: linux kernel </td>
    <td>Throughput for two way: 0.225 MPPS<br>Throughput for one way: 0.266 MPPS<br>Bottleneck: LB </td>
  </tr>
  <tr>
    <td> 2 </td>
    <td>vm1- USS -vm2</td>
    <td> ???? </td>
    <td><b>Throughput</b>(halfway: from vm to switch) : 6.50 MPPS ,<br> <b>Bottleneck</b>: USS/Jiny os start dropping, both need to improve further. <hr> <b>Throughput</b>(one way: from first vm to second vm through switch): 2.500 MPPS ,<br> <b>Bottleneck</b> : USS is a single thread and fetch one packet from queue.  <hr><b>Throughput</b>(two way: from first vm to senond vm through switch, and back to first vm): 0.400 MPPS  ,<br> <b>Bottleneck:</b> udpserver on recv side is single thread. 
        </td>
   </tr>  
   <tr>
    <td> 3 </td>
    <td>vm1- LB -vm2, <br>with multichannel vNIC(5 queues)</td>
    <td> ???? </td>
    <td> Throughput till switch: 1.000MPPS (oneway) <br>Bottleneck: LB cannot send to other side using multiqueue, send side of the switch uses only one queue, due to this the packets recved at  other end are lot less</td>
    </tr> 
       <tr>
    <td> 4 </td>
    <td>vm1- USS -vm2 <br>with multichannel vNIC(5queues)</td>
    <td> ???? </td>
    <td> ???? <br>TODO:vhost-user vNIC with multichannel is not fully implemented in kvm </td>
    </tr>
    <tr>
    <td> 5 </td>
    <td>vm1 - USS -vm2 <br>with DPDK based USS</td>
     <td>????</td>
     <td>????</td>
    </tr>
    </tr> 
    <tr>
     <td> 6 </td>
     <td>vm1- USS -vm2 <br>with VMFUNC inside vm  </td>
     <td>????</td>
     <td>????<br>
      Using Fast switch Path: Acessing other vm's virtio ring using Hardware Assist VMFUNC instruction, need changes in kvm/qemu accordingly as mentioned in the below papers. </td>
    </tr>
</table> 

##### TODO's :
With the below enhancements, the maximum throughput can be pushed further.
 
1. Low cost or low latency Notification : using the following :Monitor/wait, pause loop, posted interrupts.
2. Multi-queue with vhost-user: code is present, but need to test, this Depends on vhost-user mq support from qemu.
3. Fast-switch between vm to vm using VMFUNC. Depends on VMFUNC support from qemu/kvm and cpu support.
4. Making udp-client/server to run as High priority app, so that client,server  can process more pkts/sec. curently server is bottleneck in recving all the packets at the recving vm, it is getting dropped in the recv guest kernel itself.
5. currently USS is a single thread app,  USS need to improve further to forward more packets. 


##### Summary of Tests:
1. Test-1/2 : USS versus LB with Jiny VM's: USS as outperformed when compare to LB.
2. Test-1 : Jiny kernel versus Linux kernel: Jiny as performed better when compare to linux.This can be mitigated in Linux kernel using DPDK. The application need to change accordingly when used with DPDK. Jiny does not need DPDK, linux based application can directly run on jiny kernel without DPDK  and get the performance close to that.

##### Reasons for Better throughput in Jiny kernel when compare to linux kernel:
1. Network bottom half is very thin, and entire computation of send/recv packet is done in the application context, means the network stack runs  as part of application context concurrently without locks, this makes most of the packet processing computation on the same core and avoid locking. The implementaion of network frame work is similar to [VanJacbson paper](http://www.lemis.com/grog/Documentation/vj/lca06vj.pdf). 
2. lockless implementation of memory management in acquiring and releasing the memory buffers.
3. Poll Mode Driver (PMD): Minimising the virtio kicks in send path and in recv path using the bulk operations. Operates in Poll Mode at high rate by switching off the interupts.
4. **Area to Improve* further*: a) checksum computation takes large amount of cpu cycles, need to improve further in this area. b) using Multichannel vNIC, current kvm hypervisor does not support fully. c) third party open source networking stack from uip is used, it can be further finetuned.


##### Reasons for Better throughput in User-Space-Switch(USS) when compare to Linux-Bridge(LB):
1. lesser context switches in USS when compare to LB.
2. minimum copies, userspace switch directly copies from source vm ring to destination vm ring without intermediate copies because of shared memory. It uses shared memory between vm and switch.
3. user space switch uses huge pages, lesser TLB misses.

##### Conclusion :
1. Network throughput in virtual environment  is decided by the kernel throughput and the virtual switch connecting the vm's. The shortfall in linux kernel network throughput can be mitigated by DPDK to a larger extent, but apps need to change accordingly.
2. User Space virtual switch will outperform Kernel based switch like Linux bridge or OVS.  USS provide faster virtual infra when compare to linux bridge or switch inside the host kernel.
3. Changes in Jiny as suggested in TODO will improve the maximum throught from 4.50MPPS further.

##### Similar papers:

1. [Fast Switch using VMFUNC:](http://www.linux-kvm.org/images/8/87/02x09-Aspen-Jun_Nakajima-KVM_as_the_NFV_Hypervisor.pdf)
2. [openNFV](https://wiki.opnfv.org/vm2vm_mst)

---------------------------------------------------------------------------------- 
###Benchmark-3(Storage centric): In Progress:
  currently jiny uses  [tar file sytem](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/tar_fs.md):  

----------------------------------------------------------------------------------
###Benchmark-4(PageCache): 
   Details available in this paper ["Page cache optimizations for Hadoop".](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/PageCache-Open-Cirrus.pdf).  This paper was presented in opencirrus 2011 summit. Jiny uses same page algorithm as mentioned in the paper.
   
----------------------------------------------------------------------------------
###Benchmark-5(Malloc): In Progress
  Memory allocation improvements like zero page accumulation and other related techiniques: Based on this technique one round of testing is done but it as not given the substantial improvements as expected, need to improve and redo.
   
----------------------------------------------------------------------------------
###Benchmark-6( IPC Improvements): 

This benchmark measures InterProcess Communication(mutex,semphore,messages passing etc) in virtual environment. 

##### Problem Definition:
   During IPC, When cpu sleeps (blocking on "hlt" instruction) and getting awaken up by other cpu using IPI(Inter Processor Interrupt) within short period of time, this as performance implications for IPC and message passing workloads in virtual machines. The cost is more in vm when compare to metal because of vm exists in to hypervisor.
 
  
#####Test Environment and Results
    
  IPC Test: This is a Producer-consumer test using semaphores. Producer and consumer runs in two seperate threads. Producer is more cpu intesive when compare to consumer, this make consumer waits for producer at the end of each loop. producer wakes consumer once item is produced. In this way consumer get context switched at the end of every loop. This emulates  producer and consumer with unequal computation to process every item, this is a very common case. The [source code for test available here.] (https://github.com/naredula-jana/Jiny-Kernel/blob/master/test/expirements/sem_test4.c). If the amount of computation for producer and consumer is same then there will be minimum lock contention which will be very rare. 

 - **Hypervisor**: kvm/qemu-2.x
 - **host kernel version**:3.13.0-32
 - **command and arguments to run the test**:  "sem 800k 2000"  :  Here 800k represents the number of items produced and consumed, producer loops 2000 times in tight loop to emulate cpu consumption after every item, the more this value the more will be the consumer wait time.
     
     
 <table border="1" style="width:100%">
  <tr>
    <td><b>Test# </b></td>
    <td><b> Description</b></td>
    <td><b>Throughput</b></td>
    <td><b>Comment</b></td>
  </tr>
  <tr>
    <td>1 </td>
    <td> linux vm : kernel 3.0.x on 2 cpu,ubuntu-14  </td>
    <td> timetaken= 22 sec</td>
    <td> cpu consumption in host=150%,There are large number of IPI interrupts due to IPC test, close to 280k are generated inside the guestos </td> 
  </tr>
    <tr>
    <td>2 </td>
    <td> Jiny vm : on 2 cpu, version-2  </td>
    <td> timetaken= 9sec (140% over test-1)</td>
    <td> host cpu consumption is 200%(2 cpu's) noticed with top utility. No additional IPI interrupts are generated in the quest OS </td> 
  </tr>
    <tr>
    <td>3 </td>
    <td> linux on metal, with ubutuntu-14, more then 2 cpu  </td>
    <td> timetaken= 9.2sec (slightly less then Test-2) </td>
    <td>  </td> 
  </tr>
  <tr>
    <td>4 </td>
    <td> with ubuntu-14(linux vm - kernel 3.0.x) on 2 cpu, with cpu hogger with low priority in background </td>
    <td>Throughput: timetaken= 12sec (better then Test-1 by 83%) </td>
    <td> cpu hogger with low priority makes the cpu core  alive without going to sleep.</td> 
  </tr>
  </table>
  
          
##### **Summary of Tests:**
1. Test-1 versus Teset-2 : Jiny-vm as performed 140% better when compare to linux vm with same test setup, this is mainly interrupts are not generated because cpu avoids hitting "hlt" instruction when the thread is under wait for a short period.
2. Test-2 versus Test-3 :linux on baremetal is very close to that of jiny vm. 3% to 4% degredation is due to the IPI interrupts are generated on the metal when compare to jiny.
3. Test-1 versus Test-3 :linux kernel on the metal and on hypervisor(i,e test-1 and test-3) shows a gap of 140%. this is mainly the interrupts speed on hypervisor and hitting hlt instruction frequently. whenever "hlt" instruction is executed in guest os, control goes to kvm and get back to guest os upon wakeup, this transfer is costly when compare to small cpu spin in guest os. This is exactly implmented in jiny os to avoid hitting the "hlt" instruction. 

Issue-1: In IPC, if there is a contention in mutext or semop or other IPC, then one of the thread will be active and other thread goes to sleep. In this situation, It will be costly to wakeup a thread under sleep in virtual environment with linux  when compare to the metal or jiny os. 

##### Solution : Changes in Jiny kernel
1. In the producer and consumer test, threads will be running on seperate cpu's. when consumer thread goes to sleep due to the lock, and control goes to idle thread before hitting the "hlt" instruction, idle thread checks if there is any threads belonging to the cpu is waiting, if there is any threads under wait then sheduler spin the cpus or does a small house keeping tasks for a short duration(eg: 10ms or 1 tick) then it goes to sleep using "hlt" instruction, In this short duration if the consumer thread got woken up then it saves IPI(interrupts) generation and also exit/entry of the vcpu. In this way it saves around 800k interrupts for 800k loop in Test-2. Interrupts are costly in virtual environment when compare to metal. every interrupt causes exist and entry in the hypervisor. The "hlt" instruction casues context switch at host. 

##### When IPC based apps in linux vm can under perform due to Issue-1
1. linux as vm and with multicore(more then one cpu). The issue will not be present in single core, to minimum level on metal( shown it is only 3 to 4% as against 140%).
2. This issue will be more when the cpu's are idle relatively. example: only producer,consumer threads running on different cpu's.  If other app's are hogging the same vcpu as waiting thread then vcpu will not go to sleep and issue-1 will not popup.
3. The issue will popup if the critical section's in the app's are small and there is contention among the threads. If the critical section are large then waiter thread goes to a long sleep, in this case receving IPI interrupts is better when compare to tight cpu spin.

 Monitoring Issue-1: Due to this this, IPI interrupts will be generated in large number, this can be monitered using the number of [Rescheduling Interrupts](https://help.ubuntu.com/community/ReschedulingInterrupts) in /proc/interrupts, Rescheduling interrupts are implemented using Inter Processor Interrupt(IPI). 
 
#### Same Problem was solved differently:
Same Problem was solved by changing KVM hypervisor in this [paper](http://www.linux-kvm.org/images/a/ac/02x03-Davit_Matalack-KVM_Message_passing_Performance.pdf), published in kvm-forum-2105.  Difference between the two solutions are:

1. **Problem definition:** same in both. In this paper IPC workload is used to explain the problem, In other paper "message passing workload" is used to explain the problem.
2. **Solution to the problem:** It was fixed in guest kernel(Jiny kernel) in this paper, and the problem was solved completely. Whereas in other paper, it is fixed in hypervisor(kvm) , due to this the problem is solved partially(slide-29). 
3. Both solutions to the same problem can co-exist. since the problem is solved at two different layers(kernel and hypervisor). 

----------------------------------------------------------------------------------
##Papers:
 -   [Page cache optimizations for Hadoop, published and presented in open cirrus-2011 summit](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/PageCache-Open-Cirrus.pdf) .
 -   [Memory optimization techniques](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/malloc_paper_techpulse_submit_final.pdf).
 -   [Jiny pagecache implementation](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/pagecache.txt)
 -   [Tar Fs - Jiny root file system](https://github.com/naredula-jana/Jiny-Kernel/blob/master/doc/tar_fs.md)

##Related Projects:
 -   [Jiny Kernel](https://github.com/naredula-jana/Jiny-Kernel) .
 -   [Vmstate](https://github.com/naredula-jana/vmstate): Virtualmachine state capture and analysis.
 -   [User Space virtual Switch using Vhost-user](https://github.com/naredula-jana/Jiny-Kernel/tree/master/test/virtio-switch): Works only with kvm hypervisor.
 