##Jiny Memory Optimizations

### Memory Ballooning related optimizations:

**1. Huge Page(2M size) in Ballooning:**

The following are advantages of Huge Page when compare to 4k page:
  -  Adding and Removing the pages from/to host will be fast with Huge Page, because hypervisor(qemu) need one madvise system call for 2M page instead of 512 system calls for 4k pages.
  -  Huge pages in host machines will be fragmented because ballooning 4k pages, where as with huge page it will be intact.  THP(Transperent Huge Pages) and Virtio-ballooning will be working orthogonolly with 4k page, THP will be merging 4k pages in background, wheras ballooning breaking 2M into 4K pages. These wastage of cycles will be saved with Huge page patch.
 -  With 2M pages, Nested paging work faster because host memory will contain more Huge pages, this makes app inside the vm faster.

*Issues with Huge Page:*
  - for a small vm(with small ammount of free guest physical memory) while inflating the balloon, getting the free 2M page may be some time difficult.
  Solution: Incase 2M free page is not found, fallback to 4k page.
  
*QEMU/KVM Hypervisor Changes for Huge Page:*
  -   512 4k-pages transported to hypervisor with virtio need to check if it all belong to same 2M page, if it is then use one madvise call
   instead of 512 system calls, this is same for inflate and deflate.
   The above change is optional, This is a best effort solution. and also does not need any changes to virtio protocol between guest and kvm. The changes in the guest and qemu changes are independent, means both are compatable with older or newer versions. Huge page impact will be activated if the above changes are present in guest kernel aswell as qemu.
 
*Source code: Enable or disable Huge Page in virtio Balloon driver in [Jiny](https://github.com/naredula-jana/Jiny-Kernel):*
   -   use [MEMBALLOON_2M_PAGE_SIZE](../drivers/virtio/virtio_memballoon.cc) flag in the code to turn or off. 
 
*Benchmark* : TODO
 
### KSM and Zero page related optimizations:

 **2. Zero Pages in free list:**
 -  Clearing the free pages during idle cpu by housekeeping thread will have two advantages. Firstly, zero pages will get picked up by KSM for merging the identical pages. Secondly, maintaning sufficient zero pages in the free list will speed up the calloc calls, otherwise clearing the page will be done syncronously and increasing the delay of calloc call.  with this approach pages are zeroed asyncronously. 
 -  Example: Suppose a Vm with 8G RAM, of this 2GB is used by kernel and apps,  rest of the 6G pages are in the free list but not zero pages since it is used for some purpose, with this patch, by clearing the contents free list will contain all the zero pages, and the KSM will pick all pages in the free list. and amount memory consumed by VM is 2G+4K instead of 8G. This is compression within the VM. KSM also does the compression across the vm for identical code pages.
 
*Source code: Enable or disable zero page creation for KSM:*
   -   use [g_conf_zeropage_cache](../mm/jslab.c)  sysctl config variable to turn or off.