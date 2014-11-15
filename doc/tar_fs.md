##TarFs (Jiny's default root file system)
Tarfs: Tar ball(root.tar) is used as it is as  filesystem in Jiny. Tarfs is an extent based file system. The content or format of the file system is exactly same format of tar file, means once the file is unmounted , the file/device can be edited using tar command. 

Key concepts:

- Format: The filesystem format is exactly same as that tar file
- File Reading: It is similar to any extenet based file system where all the data os file located contnously .
- File Overwriting: No difference from any file system, offset calculation is straight format since it is extenet based.
- File Writing: This is some what the challenging problem in the tar fs. Here there are two cases for a newly created file and existing file. For the Newly created file a dummy file file with the default extent size is plalced immediatly to the file, so that as file grows in size , the same amount will shrinked in the dummy file. for the existing file, there will not be any space, so this need to be relocated.
- dummy file: Dummy files appear as ".dummy_file_offset" in tar file at the root directory or at any mentioned directory. These files are physical located next to the newly opened file. Initially the newly created file will have zerol length and dummy file next to it will have default extent size(configurable value to teh file system), As file grows the neighbouring dummy file will shrink till it reaches the default extent size. Once it crosses the extent size, the file will relocated.
- Fragmentation: This is challenging problem to solve for making tarfs an effective file system. As the new files are created, each file is reseved with the one full extent, this will waste the space, and also as the file grows behind the deault extent size, the file need to be relocated. 

Pros:

 - Format of the file system is exactly same as tar file, due to this the file system can editted offline easily with the tar command, due to this it will be very suitable file system for vm's.
 - Since this is an extent based file system, the read and writes are have better performance.

Cons:

 - Fragmentation: For a newly created files, watage of space and once file relocation are challenging problem to solve.
 - For a file system with large number of files, the meta deta need to be cached in the memory , otherwise search for a particular file is resource consuming, so tar fs is not very suitable for a large file system unless the meta data is cached durin gthe boot up  or at early stage.

Challenging Issue to solve:

 - Fragmentation: Improper  default extent size can lead to Fragmentation.
 - Getting the metadata of all file in to memory: Every file is having close to 400 bytes need to bring in to memory, If there is no cache then searching a file is have the completxity of O(n). One approach is at the kernel startup loading all the metad data in to teh cache by a house keeping thread to avoid slowdown in booting the kernel.

Configuration:

  - Default Size of extent, this is especially for the newly created files. If the extent size is large , then lot of space will wasted if the files are  small, if the extent size is small then lot resources will be wasted in relocating upon reaching the extent size.
  - Storing all the dummy files in a particular directory with some prefix: By default it is stored in root(/) with the name ".dummy_file_offset".
  

##Related Projects:
 -[Jiny](https://github.com/naredula-jana/Jiny-Kernel): Jiny Kernel.