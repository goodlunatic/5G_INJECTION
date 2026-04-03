FROM ubuntu:22.04 AS base
ARG UHD_VERSION=4.8.0.0
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ="Asia/Singapore"
ENV UHD_IMAGES_DIR=/usr/local/share/uhd/images
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
ENV PATH=/usr/local/bin:$PATH
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
# General utilities
RUN apt update && apt -y upgrade && apt -y install git nano vim wget curl iproute2 \
    cmake make ninja-build build-essential init gdb adb
RUN systemctl mask console-getty.service && \
    systemctl mask system-getty.slice && \
    systemctl mask getty@tty1.service && \
    systemctl mask getty@tty2.service && \
    systemctl mask getty@tty3.service && \
    systemctl mask getty@tty4.service && \
    systemctl mask getty@tty5.service && \
    systemctl mask getty@tty6.service && \
    systemctl mask unattended-upgrades.service

# Add UHD dependencies
FROM base AS uhd
ARG UHD_VERSION=4.8.0.0
RUN apt install -y --no-install-recommends \
    ca-certificates pkg-config python3-dev python3-mako python3-numpy python3-requests \
    libboost-all-dev libusb-1.0-0 libusb-1.0-0-dev libudev-dev
RUN git clone --branch "v${UHD_VERSION}" --depth 1 https://github.com/EttusResearch/uhd.git /tmp/uhd && \
    cmake -S /tmp/uhd/host -B /tmp/uhd/build \
        -DENABLE_PYTHON_API=OFF \
        -DENABLE_MANUAL=OFF \
        -DENABLE_TESTS=OFF && \
    cmake --build /tmp/uhd/build -j"$(nproc)" && \
    cmake --install /tmp/uhd/build && \
    mkdir -p /usr/lib/x86_64-linux-gnu && \
    ln -sf /usr/local/lib/libuhd.so /usr/lib/x86_64-linux-gnu/libuhd.so && \
    ln -sf "/usr/local/lib/libuhd.so.${UHD_VERSION}" "/usr/lib/x86_64-linux-gnu/libuhd.so.${UHD_VERSION}" && \
    ldconfig && \
    uhd_images_downloader -t x3xx && \
    rm -rf /tmp/uhd

# Add srsran 4G dependencies
FROM uhd AS srsran_4g_base
RUN apt -y install libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev

# Add srsRAN project
FROM uhd AS srsran5g
RUN apt -y install gcc g++ pkg-config libfftw3-dev libmbedtls-dev libsctp-dev libyaml-cpp-dev libgtest-dev
RUN git clone https://github.com/srsRAN/srsRAN_Project.git /root/srsran
WORKDIR /root/srsran
RUN git checkout release_24_10_1 && cmake -B build -G Ninja && ninja -C build

# Add open5gs project
FROM base AS open5gs
RUN wget -qO- https://www.mongodb.org/static/pgp/server-8.0.asc | tee /etc/apt/trusted.gpg.d/server-8.0.asc && \
    echo "deb [ arch=amd64,arm64 ] https://repo.mongodb.org/apt/ubuntu jammy/mongodb-org/8.0 multiverse" | tee /etc/apt/sources.list.d/mongodb-org-8.0.list && \
    apt update && apt install -y mongodb-mongosh
RUN apt -y install python3-pip python3-setuptools python3-wheel flex bison \
    libsctp-dev libgnutls28-dev libgcrypt-dev libssl-dev libmongoc-dev libbson-dev libyaml-dev libnghttp2-dev \
    libmicrohttpd-dev libcurl4-gnutls-dev libnghttp2-dev libtins-dev libtalloc-dev meson && \
    apt install -y --no-install-recommends libidn-dev
RUN git clone https://github.com/open5gs/open5gs /root/open5gs
WORKDIR /root/open5gs
RUN meson build --prefix=`pwd`/install && ninja -C build
RUN curl -fsSL https://deb.nodesource.com/setup_20.x -o nodesource_setup.sh && \
    bash nodesource_setup.sh && \
    apt install nodejs -y && \
    rm nodesource_setup.sh
RUN cd webui && npm install

# Add wdissector
FROM srsran_4g_base AS wdissector
# Install dependencies for 5Ghoul
RUN apt -y install sudo init python3-pip python3-dev software-properties-common kmod bc gzip zstd flex bison \
        pkg-config swig graphviz libglib2.0-dev libgcrypt20-dev libnl-genl-3-200 libgraphviz-dev liblz4-dev \
        libsnappy-dev libgnutls28-dev libxml2-dev libnghttp2-dev libkrb5-dev libnl-3-dev libspandsp-dev \
        libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libnl-genl-3-dev libnl-route-3-dev \
        libnl-nf-3-dev libcap-dev libbrotli-dev libsmi2-dev liblua5.2-dev libc-ares-dev libsbc-dev \
        libspeexdsp-dev libfreetype6-dev libxss1 libtbb-dev libnss3-dev libudev-dev libpulse-dev \
        libpcre2-dev libasound2-dev libgl1-mesa-dev libssh-dev libmaxminddb-dev libopus-dev \
        libusb-1.0-0 psmisc sshpass tcpdump libfmt-dev libsodium-dev xxd libc-bin libmbim-glib-dev \
        libgoogle-glog-dev libzstd-dev libevent-dev libunwind-dev libdouble-conversion-dev libgflags-dev \
        qtbase5-dev libqt5multimedia5 libqt5svg5 qttools5-dev qtmultimedia5-dev g++ software-properties-common \
        kmod libglib2.0-dev libsnappy1v5 libsmi2ldbl liblua5.2-0 libc-ares2 libnl-route-3-200 libnl-genl-3-200 \
        libfreetype6 graphviz libtbb12 libxss1 libnss3 libspandsp2 libsbc1 libbrotli1 libnghttp2-14 libasound2 \
        psmisc sshpass libpulse0 libasound2 libpcre2-dev libmaxminddb0 libopus0 libspeex1 bc tcpdump libgflags2.2 \
        libzstd1 libunwind8 libcap2 libspeexdsp1 libxtst6 libatk-bridge2.0-0 libusb-1.0-0 meson
# Install llvm 15
RUN wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh && ./llvm.sh 15 && rm llvm.sh
RUN arch=$(dpkg --print-architecture) \
    && libssl_pkg="libssl1.1_1.1.1w-0+deb11u5_${arch}.deb" \
    && wget "https://security.debian.org/pool/updates/main/o/openssl/${libssl_pkg}" \
    && dpkg -i "${libssl_pkg}" \
    && rm "${libssl_pkg}"

# Clone and build wdissector
RUN git clone https://github.com/asset-group/5ghoul-5g-nr-attacks /root/wdissector
WORKDIR /root/wdissector
RUN ./scripts/apply_patches.sh 3rd-party
RUN cd 3rd-party/ && git clone https://github.com/zeromq/libzmq.git --depth=1 && cd libzmq && \
    ./autogen.sh && ./configure && make -j && make install && ldconfig
RUN cd 3rd-party/ && git clone https://github.com/zeromq/czmq.git --depth=1 && cd czmq && \
        ./autogen.sh && ./configure && make -j && make install && ldconfig
RUN cd 3rd-party/ && git clone https://github.com/json-c/json-c.git --depth=1 && cd json-c && \
    mkdir -p build && cd build && cmake ../ && make -j && make install && ldconfig
RUN curl https://raw.githubusercontent.com/jckarter/tbb/refs/heads/master/include/tbb/tbb_stddef.h -o /usr/include/tbb/tbb_stddef.h
RUN sed -i 's/\blong[[:space:]]\+gettid()/__pid_t gettid()/g' /root/wdissector/src/MiscUtils.hpp && \
    sed -i 's/\bif\s*(\s*SWIG_EXIST\s*)/if(NOT SWIG_EXIST)/g' /root/wdissector/CMakeLists.txt && \
    sed -i 's/exit 1/# exit 1/' /root/wdissector/build.sh 
RUN cd /root/wdissector && ./build.sh all

FROM wdissector AS sni5gect
RUN apt install -y build-essential libfftw3-dev libmbedtls-dev libboost-program-options-dev libconfig++-dev libsctp-dev libzmq3-dev libliquid-dev libyaml-cpp-dev
RUN git clone https://github.com/asset-group/Sni5Gect-5GNR-sniffing-and-exploitation.git /root/sni5gect
WORKDIR /root/sni5gect

FROM sni5gect AS sni5gect-dev
# # Install cuda
# RUN wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb && \
#     dpkg -i cuda-keyring_1.1-1_all.deb && \
#     apt update && apt -y install cuda-toolkit-12-8 && rm cuda-keyring_1.1-1_all.deb
# ENV PATH=/usr/local/cuda-12.8/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

# Install miniconda
RUN arch=$(uname -m) && \
    if [ "$arch" = "x86_64" ]; then arch="x86_64"; elif [ "$arch" = "aarch64" ]; then arch="aarch64"; fi && \
    wget "https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-${arch}.sh" && \
    bash "Miniconda3-latest-Linux-${arch}.sh" -p /root/.miniconda -b -u && \
    /root/.miniconda/bin/conda init bash && \
    rm "Miniconda3-latest-Linux-${arch}.sh"

# Install qcsuper
RUN . "/root/.miniconda/etc/profile.d/conda.sh" && conda activate base && \
    pip install pandas libtmux loguru seaborn jupyter ipython ipykernel pyserial pyusb crcmod pycrate && \
    git clone https://github.com/P1sec/QCSuper /root/qcsuper && \
    cd /root/qcsuper && pip3 install --upgrade https://github.com/P1sec/pycrate/archive/master.zip
RUN apt install -y cutecom tmux

# build
RUN cmake -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=TRUE \
        -DCMAKE_C_COMPILER=/usr/bin/clang-15 \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++-15 \
        --no-warn-unused-cli -B build -G Ninja && \
        ninja -C build
