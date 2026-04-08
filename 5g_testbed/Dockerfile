FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Asia/Shanghai
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
ENV UHD_IMAGES_DIR=/usr/share/uhd/images

# ---- 使用 USTC 国内源 ----
RUN sed -i 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.ustc.edu.cn/ubuntu/|g; s|http://security.ubuntu.com/ubuntu/|http://mirrors.ustc.edu.cn/ubuntu/|g' /etc/apt/sources.list

# ---- 安装依赖 ----
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ccache \
    cmake \
    doxygen \
    git \
    ninja-build \
    pkg-config \
    ca-certificates \
    curl \
    gnupg \
    lsb-release \
    software-properties-common \
    libfftw3-dev \
    libboost-dev \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-thread-dev \
    libboost-program-options-dev \
    libmbedtls-dev \
    libsctp-dev \
    libconfig-dev \
    libconfig++-dev \
    rapidjson-dev \
    libyaml-cpp-dev \
    libzmq3-dev \
    libdw-dev \
    binutils-dev \
    libdwarf-dev \
    libelf-dev \
    libnuma-dev \
    iproute2 \
    iputils-ping \
    net-tools \
    usbutils \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ---- UHD ----
RUN add-apt-repository -y ppa:ettusresearch/uhd \
    && apt-get update --fix-missing \
    && apt-get install -y libuhd-dev uhd-host \
    && uhd_images_downloader

WORKDIR /home/workspace

CMD ["/bin/bash"]
