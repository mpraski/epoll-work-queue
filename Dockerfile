FROM ubuntu:22.10
COPY ./ ./src

RUN apt-get update && apt-get -y install sudo wget
RUN sudo apt install -y g++ libcurl4-openssl-dev make
RUN wget https://github.com/Kitware/CMake/releases/download/v3.17.2/cmake-3.17.2-Linux-x86_64.sh \
      -q -O /tmp/cmake-install.sh \
      && chmod u+x /tmp/cmake-install.sh \
      && mkdir /usr/bin/cmake \
      && /tmp/cmake-install.sh --skip-license --prefix=/usr/bin/cmake \
      && rm /tmp/cmake-install.sh
ENV PATH="/usr/bin/cmake/bin:${PATH}"

WORKDIR /src
RUN rm -rf cmake-build-debug
RUN mkdir -p cmake-build-debug && cd cmake-build-debug && cmake .. && make