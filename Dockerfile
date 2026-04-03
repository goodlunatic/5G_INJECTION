FROM ubuntu:22.04

ARG UHD_VERSION=4.8.0.0

ENV DEBIAN_FRONTEND=noninteractive
ENV LC_ALL=C.UTF-8
ENV LANG=C.UTF-8
ENV TZ=Asia/Shanghai
ENV UHD_IMAGES_DIR=/usr/local/share/uhd/images
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/local/lib/x86_64-linux-gnu
ENV PATH=/usr/local/bin:$PATH

SHELL ["/bin/bash", "-lc"]

RUN ln -snf /usr/share/zoneinfo/${TZ} /etc/localtime \
    && echo "${TZ}" > /etc/timezone \
    && sed -i 's/archive.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list \
    && sed -i 's/security.ubuntu.com/mirrors.ustc.edu.cn/g' /etc/apt/sources.list

RUN apt-get update --fix-missing && apt-get install -y --no-install-recommends \
    software-properties-common ca-certificates curl gnupg wget lsb-release apt-transport-https \
    autoconf automake build-essential ccache cmake pkg-config ninja-build git vim nano tmux screen \
    cpufrequtils doxygen ethtool fort77 g++ gdb gobject-introspection gpsd gpsd-clients init \
    inetutils-tools iproute2 iputils-ping net-tools python3-dev python3-docutils python3-gi \
    python3-gi-cairo python3-gps python3-lxml python3-mako python3-numpy python3-opengl \
    python3-pyqt5 python3-requests python3-ruamel.yaml python3-scipy python3-setuptools \
    python3-six python3-sphinx python3-yaml python3-zmq swig \
    libasound2-dev libboost-all-dev libboost-program-options-dev libcomedi-dev libconfig++-dev \
    libconfig-dev libcppunit-dev libcurl4-openssl-dev libfftw3-bin libfftw3-dev libfftw3-doc \
    libfontconfig1-dev libglib2.0-dev libgmp-dev libgps-dev libgsl-dev libliquid-dev \
    liblog4cpp5-dev libmbedtls-dev libncurses5 libncurses5-dev libpulse-dev \
    libqt5charts5-dev libqt5opengl5-dev libqwt-qt5-dev libsctp-dev libsdl1.2-dev libtool \
    libudev-dev libusb-1.0-0 libusb-1.0-0-dev libxi-dev libxrender-dev libyaml-cpp-dev \
    libzmq3-dev qtbase5-dev qtdeclarative5-dev \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --branch "v${UHD_VERSION}" --depth 1 https://github.com/EttusResearch/uhd.git /tmp/uhd \
    && cmake -S /tmp/uhd/host -B /tmp/uhd/build \
         -DENABLE_PYTHON_API=OFF \
         -DENABLE_MANUAL=OFF \
         -DENABLE_TESTS=OFF \
    && cmake --build /tmp/uhd/build -j"$(nproc)" \
    && cmake --install /tmp/uhd/build \
    && mkdir -p /usr/lib/x86_64-linux-gnu \
    && ln -sf /usr/local/lib/libuhd.so /usr/lib/x86_64-linux-gnu/libuhd.so \
    && ln -sf "/usr/local/lib/libuhd.so.${UHD_VERSION}" "/usr/lib/x86_64-linux-gnu/libuhd.so.${UHD_VERSION}" \
    && ldconfig \
    && uhd_images_downloader -t x3xx \
    && rm -rf /tmp/uhd

RUN git clone --depth 1 https://github.com/jgaeddert/liquid-dsp.git /tmp/liquid-dsp \
    && cd /tmp/liquid-dsp \
    && ./bootstrap.sh \
    && ./configure \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig \
    && rm -rf /tmp/liquid-dsp

RUN git clone --depth 1 https://github.com/srsran/srsGUI.git /tmp/srsGUI \
    && mkdir -p /tmp/srsGUI/build \
    && cd /tmp/srsGUI/build \
    && cmake .. \
    && make -j"$(nproc)" \
    && make install \
    && ldconfig \
    && rm -rf /tmp/srsGUI

RUN mkdir -p /etc/udev/rules.d \
    && echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="2500", MODE="0666"' > /etc/udev/rules.d/90-usrp.rules

WORKDIR /workspace

CMD ["/bin/bash"]
