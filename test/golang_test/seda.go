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
func process1(in chan int,out chan int ) {
	 buf := make([]byte, 1024)
  for {
    msg := <- in
    //fmt.Println(msg)
   // time.Sleep(time.Millisecond * 1)
   count, _ := fo.Write(buf[:30])
    out <- msg*2*count*spin(1000)
  }
}

func process2(in chan int,out chan int ) {
	 buf := make([]byte, 1024)
  for {
    msg := <- in
    count, _ := fo.Write(buf[:30])
    //fmt.Println(msg)
    //time.Sleep(time.Millisecond * 1)
    out <- msg*5*count*spin(1000)
  }
}
func process3(in chan int,out chan int ) {
	 buf := make([]byte, 1024)
  for {
    msg := <- in
    count, _ := fo.Write(buf[:30])
    //fmt.Println(msg)
    //time.Sleep(time.Millisecond * 1)
    out <- msg*2*count*spin(1000)
  }
}
func process4(in chan int,out chan int ) {
	 buf := make([]byte, 1024)
  for {
    msg := <- in
    count, _ := fo.Write(buf[:30])
    //fmt.Println(msg)
    //time.Sleep(time.Millisecond * 10)
    out <- msg*1*count*spin(1000)
  }
}
func main() {
    maxCount  := flag.Int("count", 1000, "max count")
    flag.Parse()
    fmt.Println("SEDA:  SAMPLE SEDA application with files and channels: ",*maxCount)

    arg := os.Args

    fmt.Println("SEDA:   Arguments to application  :",arg)

    in := make(chan int)
    out := make(chan int)
    
    p1 := make(chan int)
    p2 := make(chan int)
    p3 := make(chan int)
    defer close(p1)
    defer close(p2)
    defer close(p3)
    defer close(in)
    defer close(out)

    fo, _ = os.Create("/dev/null")

    go process1(in, p1)
    go process2(p1, p2)
    go process3(p2, p3)
    go process4(p3, out)

    total:=0
    
    for i:=0; i<*maxCount; i++ {
    	
	       in <- i
    	   in <- i+1
    	   in <- i+2
    	   in <- i
    	
	       k :=<- out
	       k =<- out
	       k =<- out
	       k =<- out
		
       
       //fmt.Println(k)
       total=total+k
     }
     fmt.Println("SEDA: Final total: ",total)
}
