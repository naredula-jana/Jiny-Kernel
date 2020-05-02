package main

import (
    "fmt"
   "flag"
   "os"
   "runtime"
 //   "time"
)

var fo *os.File
var ready int
func spin(arg int) int {
	ret:=0
	for k:=0; k<arg*10; k++ {
		ret=ret+arg
	}
	return ret;
}
func process(in chan int,out chan int ) {
	 buf := make([]byte, 1024)
	 for ready < 1 {
	 	runtime.Gosched()
	 }
    for {
    msg := <- in
 
    count, _ := fo.Write(buf[:30])
    out <- msg*2*count*spin(100)
  }
}

func main() {
    maxCount  := flag.Int("count", 1000, "max count")
    flag.Parse()
    fmt.Println("SERVER:  SAMPLE application with files and channels: ",*maxCount)
    arg := os.Args
    fmt.Println("SERVER:   Arguments to application  :",arg)

    fo, _ = os.Create("/dev/null")
    buf := make([]byte, 1024)
    maxGoroutines :=50
    var in [50]chan int
    var out [50]chan int
    ready=0
    
    for i:=0; i<maxGoroutines; i++ {
	    in[i] = make(chan int)
	    out[i] = make(chan int)

        go process(in[i], out[i])
    }

    total:=0
    k:=0
    ready=1
    
    
    //fmt.Println("SERVER Start Time: ", time.Now().Format(time.RFC850))
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
    for i:=0; i<maxGoroutines; i++ {
    	defer close(in[i])
        defer close(out[i])
    }
    //fmt.Println("SERVER End Time: ", time.Now().Format(time.RFC850))
    fmt.Println("SERVER: Final total: ",total)
}
