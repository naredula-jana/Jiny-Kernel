package main

import "fmt"
import "os"
import "time"
//import "io/ioutil"
/*
ON LINUX:
janardhana.reddy@megh-none-393124:~$ ./go_file
Thursday, 28-Apr-16 10:38:11 IST
total data count : 2700000000  write count:90000000
Thursday, 28-Apr-16 10:38:32 IST

total time taken on linux : 21 sec
------------------------

ON JINY as Normal APP
/ $ data/go_file
Thursday, 28-Apr-16 04:54:21 UTC
total data count : 2700000000  write count:90000000
Thursday, 28-Apr-16 04:54:42 UTC

total time taken as Normal APP : 21 sec
------------
ON JINY as Highpriority APP
-->insexe data/file
 Successfull loaded the high priority app
-->Thursday, 28-Apr-16 04:57:14 UTC
total data count : 2700000000  write count:90000000
Thursday, 28-Apr-16 04:57:22 UTC

total time taken as High Priority APP : 8 sec
*/

func main() {
	i:=0
	fo, _ := os.Create("/dev/null")
    buf := make([]byte, 1024)
    total:=0;
  fmt.Println(time.Now().Format(time.RFC850))
    for i = 0; i < 90000000; i++ {
    	count, _ := fo.Write(buf[:30])
    	total =total+count
    }
	fmt.Printf("total data count : %d  write count:%d\n",total,i)
	 fmt.Println(time.Now().Format(time.RFC850))
}