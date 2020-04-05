
**Procedure to Build the Image from master and run:**

Below Docker Container will pull the code from master branch, after that it build the image and then execute the image. 

```
 docker run --privileged --rm -it --entrypoint "/run/jiny_compile"  naredulajana/jiny:latest

```

**Procedure to Run the default Image:**

Below Docker Container will execute the default image from master branch. 

```
  docker run --rm -it  naredulajana/jiny:latest
```
