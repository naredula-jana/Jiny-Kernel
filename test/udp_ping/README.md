##udp ping 
This tool contain two programs **udp_client** and **udp_server**.  udp_server listens on a udp port, once it receives the packt on the port it simply bounce back the packet to the replier on the same port it as recvied from.
udp_client does all the hard work. It sends n number of packets in a duration of 30 seconds. The packets are send in a equal distance, intra packet distance is 30/n seconds. there will 2 threads in udp client one sending and other for collecting the packets. At the end of 30 seconds udp ping calculates the number of packet recvied back it own packets from the udp server, there by it calculates the bandwidth it as able to sucessfully send.

 ** ./udp_client <udp_server_ip> <packet_length> <number of pkts> **
 
 udp_client sends packets for 30 seconds, the spacing between the packets is determined by the number of packets. 

 ** ./udp_server ** 
 
 
 TODO in udp_client:
  
 1.  Calculating average jitter time between the packet send and recv.
 2.  Calculating average roundtrip time.
  
