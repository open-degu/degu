FROM ubuntu:18.04

MAINTAINER AtmarkTechno, Inc.

RUN set -x && \
apt-get -y update && \ 
apt-get -y install openssh-server \
cmake \
ccache doxygen dfu-util device-tree-compiler \
python3-ply python3-pip python3-setuptools python3-wheel xz-utils file \
make gcc-arm-none-eabi autoconf automake libtool

ENV HOSTNAME degu-builder

RUN useradd -m -s /bin/bash devel && \
echo 'devel:devel' | chpasswd

RUN sed -ri 's/UsePAM yes/#UsePAM yes/g' /etc/ssh/sshd_config
RUN sed -ri 's/#UsePAM no/UsePAM no/g' /etc/ssh/sshd_config
EXPOSE 22

RUN wget https://github.com/zephyrproject-rtos/meta-zephyr-sdk/releases/download/0.9.5/zephyr-sdk-0.9.5-setup.run
RUN sh zephyr-sdk-0.9.5-setup.run

#COPY api-server /opt/api-server
#CMD ["/opt/api-server/server_start"]

# docker run \
# --ip=192.168.56.100 \
# --name=spus \
