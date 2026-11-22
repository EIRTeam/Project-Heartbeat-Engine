FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl git gcc-12 g++-12 make nasm pkg-config \
    python3 python3-pip binutils xz-utils \
    libx11-dev libxcursor-dev libxinerama-dev libxi-dev libxrandr-dev \
    libasound2-dev libpulse-dev libudev-dev libfontconfig1-dev \
    && rm -rf /var/lib/apt/lists/*
RUN python3 -m pip install --no-cache-dir scons==4.11.1 && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 120 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 120 && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 120 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 120
ENV CC=gcc CXX=g++
ENTRYPOINT ["/bin/bash", "/src/misc/linux_build_container/vulkan_tester/build.sh"]
