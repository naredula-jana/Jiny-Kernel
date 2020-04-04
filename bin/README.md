
**procedure to Build the Image**

```
  cd <directory where Jiny source code is installed >
  make all
  cd bin
  ./update_image 
  docker build --tag naredulajana/jiny_run .
  docker push naredulajana/jiny_run
```

**procedure to Run the Image**

```
  docker run --rm -it naredulajana/jiny_run:latest
```
