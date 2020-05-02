package main

import "fmt"
import "os"
import "time"
import  "flag"

func main() {
	
	maxCount  := flag.Int("count", 1000, "max count")
	flag.Parse()
	fmt.Println("FileApp:  SAMPLE File application count: ",*maxCount)
	 
	i:=0
	fo, _ := os.Create("/dev/null")
    buf := make([]byte, 1024)
    total:=0;
    
    fmt.Println("New Start Time:New New ", time.Now().Format(time.RFC850))
    for i = 0; i < *maxCount; i++ {
    	count, _ := fo.Write(buf[:30])
    	total =total+count
    }
	fmt.Printf("total data count : %d  write count:%d\n",total,i)
	fmt.Println("End Time:",time.Now().Format(time.RFC850))
}
