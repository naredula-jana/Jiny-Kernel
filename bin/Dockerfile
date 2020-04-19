FROM debian:buster-20190228-slim

RUN apt-get update && apt-get -y upgrade && \
    apt-get --no-install-recommends -y install \
        iproute2 \
        wget \
        unzip \
        make \
        jq \
        tar \
       gcc \
        sudo \
#        python3 \
        qemu-system-x86 \
        udhcpd \
    && apt-get clean

# RUN apt-get install gcc-4.8

COPY Docker_master_run.sh /run/master_run
COPY Docker_run.sh /run/jiny_run
COPY Docker_local_compile_run.sh /run/local_compile_run
COPY Docker_local_run.sh /run/local_run
COPY addon_disk /run/
COPY disk.img /run/


# TODO:  docker build will fail due to absence of this file, move the below large file in some repo
ADD gcc-4.8.2.tar.gz /opt/
COPY gcc_files/libmpfr.so.4 /usr/lib/x86_64-linux-gnu/
COPY gcc_files/libmpc.so.3 /usr/lib/x86_64-linux-gnu/
ADD gcc_files/include.tar.gz /


VOLUME /image

ENTRYPOINT ["/run/jiny_run"]

# Mostly users will probably want to configure memory usage.
CMD ["-m", "512M"]
