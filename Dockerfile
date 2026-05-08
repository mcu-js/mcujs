FROM alpine:3.19

# Install build dependencies in a single layer and clean up
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    python3 \
    bash \
    newlib-arm-none-eabi \
    gcc-arm-none-eabi \
    g++-arm-none-eabi \
    linux-headers \
    bsd-compat-headers

# Clone Pico SDK 2.2.0
ENV PICO_SDK_PATH=/opt/pico-sdk
RUN git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git ${PICO_SDK_PATH} \
    && cd ${PICO_SDK_PATH} \
    && git submodule update --init --recursive --depth 1

# Build picotool during image creation so firmware builds do not fetch it.
ENV PICOTOOL_VERSION=2.2.0
ENV PICOTOOL_SOURCE_PATH=/opt/picotool-src
ENV picotool_DIR=/opt/picotool/picotool
RUN git clone --depth 1 --branch ${PICOTOOL_VERSION} https://github.com/raspberrypi/picotool.git ${PICOTOOL_SOURCE_PATH} \
    && cmake -S ${PICOTOOL_SOURCE_PATH} -B ${PICOTOOL_SOURCE_PATH}/build \
        -DPICO_SDK_PATH=${PICO_SDK_PATH} \
        -DPICOTOOL_NO_LIBUSB=1 \
        -DPICOTOOL_FLAT_INSTALL=1 \
        -DCMAKE_INSTALL_PREFIX=/opt/picotool \
    && cmake --build ${PICOTOOL_SOURCE_PATH}/build --target install --parallel \
    && rm -rf ${PICOTOOL_SOURCE_PATH}

# Clone JerryScript
ENV JERRYSCRIPT_PATH=/opt/jerryscript
RUN git clone --depth 1 --branch v3.0.0 https://github.com/pando-project/jerryscript.git ${JERRYSCRIPT_PATH}

# Download FatFs R0.16
ENV FATFS_PATH=/opt/fatfs
RUN mkdir -p ${FATFS_PATH} \
    && wget -q http://elm-chan.org/fsw/ff/arc/ff16.zip -O /tmp/ff16.zip \
    && unzip -q /tmp/ff16.zip -d ${FATFS_PATH} \
    && rm /tmp/ff16.zip \
    && rm ${FATFS_PATH}/source/ffconf.h  # Remove default config, projects must provide their own

# Download picojpeg v1.1
ENV PICOJPEG_PATH=/opt/picojpeg
RUN mkdir -p ${PICOJPEG_PATH} \
    && wget -q https://raw.githubusercontent.com/richgel999/picojpeg/master/picojpeg.c -O ${PICOJPEG_PATH}/picojpeg.c \
    && wget -q https://raw.githubusercontent.com/richgel999/picojpeg/master/picojpeg.h -O ${PICOJPEG_PATH}/picojpeg.h

# Clone PicoDVI for DVI/HDMI output support
ENV PICODVI_PATH=/opt/picodvi
RUN git clone --depth 1 https://github.com/Wren6991/PicoDVI.git ${PICODVI_PATH}

# Set working directory
WORKDIR /workspace

# Entry point
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["all"]
