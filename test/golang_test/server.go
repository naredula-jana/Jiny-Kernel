package main

import (
    "fmt"
   "flag"
   "os"
//    "time"
)

var fo *os.File

func spin(arg int) int {
	ret:=0
	for k:=0; k<arg*10; k++ {
		ret=ret+arg
	}
	return ret;
}
func process(in chan int,out chan int ) {
	 buf := make([]byte, 1024)
  for {
    msg := <- in
    //fmt.Println(msg)
   // time.Sleep(time.Millisecond * 1)
    count, _ := fo.Write(buf[:30])
    out <- msg*2*count*spin(100)
  }
}

func main() {
    maxCount  := flag.Int("count", 1000, "max count")
    flag.Parse()
    fmt.Println("SERVER:  SAMPLE SEDA application with files and channels: ",*maxCount)
    arg := os.Args
    fmt.Println("SERVER:   Arguments to application  :",arg)

    fo, _ = os.Create("/dev/null")
    buf := make([]byte, 1024)
    maxGoroutines :=50
    var in [50]chan int
    var out [50]chan int
    for i:=0; i<maxGoroutines; i++ {
	    in[i] = make(chan int)
	    out[i] = make(chan int)
	    defer close(in[i])
        defer close(out[i])
        go process(in[i], out[i])
    }

    total:=0
    k:=0
    
    for i:=0; i<*maxCount; i++ {
    	for j:=0; j<maxGoroutines; j++ {
	       in[j] <- i
    	}
    	for j:=0; j<maxGoroutines; j++ {
	       _, _ = fo.Write(buf[:30])
    	}
    	for j:=0; j<maxGoroutines; j++ {
	       k=<- out[j]
           total=total+k
    	}
     }
     fmt.Println("SERVER: Final total: ",total)
}
