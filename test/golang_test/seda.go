package main

import (
    "fmt"
   "flag"
   "os"
//    "time"
)

func process1(in chan int,out chan int ) {
  for {
    msg := <- in
    //fmt.Println(msg)
   // time.Sleep(time.Millisecond * 1)
    out <- msg*2
  }
}

func process2(in chan int,out chan int ) {
  for {
    msg := <- in
    //fmt.Println(msg)
    //time.Sleep(time.Millisecond * 1)
    out <- msg*5
  }
}
func process3(in chan int,out chan int ) {
  for {
    msg := <- in
    //fmt.Println(msg)
    //time.Sleep(time.Millisecond * 1)
    out <- msg*2
  }
}
func process4(in chan int,out chan int ) {
  for {
    msg := <- in
    //fmt.Println(msg)
    //time.Sleep(time.Millisecond * 10)
    out <- msg*1
  }
}
func main() {
    maxCount  := flag.Int("count", 1000, "max count")
    flag.Parse()
    fmt.Println("SEDA: Sample Go Channel for SEDA architecture: ",*maxCount)

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

    go process1(in, p1)
    go process2(p1, p2)
    go process3(p2, p3)
    go process4(p3, out)

    total:=0
    for i:=0; i<*maxCount; i++ {
       in <- i
       k :=<- out
       //fmt.Println(k)
       total=total+k
     }
     fmt.Println("SEDA: Final total: ",total)
}
