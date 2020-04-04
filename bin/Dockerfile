FROM debian:buster-20190228-slim

RUN apt-get update && apt-get -y upgrade && \
    apt-get --no-install-recommends -y install \
        iproute2 \
        jq \
        python3 \
        qemu-system-x86 \
        udhcpd \
    && apt-get clean

COPY Docker_run.sh /run/
COPY addon_disk /run/
COPY disk.img /run/

VOLUME /image

ENTRYPOINT ["/run/Docker_run.sh"]

# Mostly users will probably want to configure memory usage.
CMD ["-m", "512M"]
