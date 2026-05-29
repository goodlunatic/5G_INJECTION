FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Asia/Shanghai \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8 \
    UHD_IMAGES_DIR=/usr/share/uhd/images

# 使用 USTC 镜像源
RUN sed -i \
    -e 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.ustc.edu.cn/ubuntu/|g' \
    -e 's|http://security.ubuntu.com/ubuntu/|http://mirrors.ustc.edu.cn/ubuntu/|g' \
    /etc/apt/sources.list

# 安装依赖
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential ccache cmake doxygen git ninja-build pkg-config \
    ca-certificates curl gnupg lsb-release software-properties-common \
    libfftw3-dev libboost-dev libboost-system-dev \
    libboost-filesystem-dev libboost-thread-dev \
    libboost-program-options-dev libboost-test-dev \
    libmbedtls-dev libsctp-dev \
    libconfig-dev libconfig++-dev \
    rapidjson-dev libyaml-cpp-dev libzmq3-dev \
    libdw-dev binutils-dev libdwarf-dev libelf-dev libnuma-dev \
    libusb-1.0-0-dev python3-dev \
    python3-mako python3-numpy python3-requests python3-ruamel.yaml python3-setuptools \
    iproute2 iputils-ping net-tools usbutils \
 && rm -rf /var/lib/apt/lists/*

# 编译安装 UHD 4.8.0.0
RUN git clone --depth 1 --branch v4.8.0.0 \
      https://github.com/EttusResearch/uhd.git /tmp/uhd \
 && cmake -S /tmp/uhd/host -B /tmp/uhd/build \
      -G Ninja \
      -DCMAKE_INSTALL_PREFIX=/usr \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_TESTS=OFF \
      -DENABLE_EXAMPLES=ON \
      -DENABLE_MANUAL=OFF \
      -DENABLE_DOXYGEN=OFF \
 && cmake --build /tmp/uhd/build -j"$(nproc)" \
 && cmake --install /tmp/uhd/build \
 && ldconfig \
 && uhd_images_downloader \
 && rm -rf /tmp/uhd

WORKDIR /home/workspace

CMD ["/bin/bash"]
