# Multi-platform index, not an architecture-specific manifest. See dependency notes.
FROM alpine:3.19@sha256:6baf43584bcb78f2e5847d1de515f23499913ac9f12bdf834811a3145eb11ca1

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
ENV PICO_SDK_COMMIT=a1438dff1d38bd9c65dbd693f0e5db4b9ae91779
RUN git init ${PICO_SDK_PATH} \
    && cd ${PICO_SDK_PATH} \
    && git remote add origin https://github.com/raspberrypi/pico-sdk.git \
    && git fetch --depth 1 origin ${PICO_SDK_COMMIT} \
    && git checkout --detach FETCH_HEAD \
    && test "$(git rev-parse HEAD)" = "${PICO_SDK_COMMIT}" \
    && git submodule update --init --recursive --depth 1

# Build picotool during image creation so firmware builds do not fetch it.
ENV PICOTOOL_VERSION=2.2.0
ENV PICOTOOL_SOURCE_PATH=/opt/picotool-src
ENV picotool_DIR=/opt/picotool/picotool
ENV PICOTOOL_COMMIT=a7eb3988f0645239185fadb4e25d8279478c2dbb
RUN git init ${PICOTOOL_SOURCE_PATH} \
    && cd ${PICOTOOL_SOURCE_PATH} \
    && git remote add origin https://github.com/raspberrypi/picotool.git \
    && git fetch --depth 1 origin ${PICOTOOL_COMMIT} \
    && git checkout --detach FETCH_HEAD \
    && test "$(git rev-parse HEAD)" = "${PICOTOOL_COMMIT}" \
    && cmake -S ${PICOTOOL_SOURCE_PATH} -B ${PICOTOOL_SOURCE_PATH}/build \
        -DPICO_SDK_PATH=${PICO_SDK_PATH} \
        -DPICOTOOL_NO_LIBUSB=1 \
        -DPICOTOOL_FLAT_INSTALL=1 \
        -DCMAKE_INSTALL_PREFIX=/opt/picotool \
    && cmake --build ${PICOTOOL_SOURCE_PATH}/build --target install --parallel \
    && rm -rf ${PICOTOOL_SOURCE_PATH}

# Clone JerryScript 3.0.0
ENV JERRYSCRIPT_PATH=/opt/jerryscript
ENV JERRYSCRIPT_COMMIT=50200152feb724a74a5f64e44d7885151537cfad
RUN git init ${JERRYSCRIPT_PATH} \
    && cd ${JERRYSCRIPT_PATH} \
    && git remote add origin https://github.com/pando-project/jerryscript.git \
    && git fetch --depth 1 origin ${JERRYSCRIPT_COMMIT} \
    && git checkout --detach FETCH_HEAD \
    && test "$(git rev-parse HEAD)" = "${JERRYSCRIPT_COMMIT}"

# Download FatFs R0.16
# Remove its default ffconf.h after extraction; projects provide their own config.
ENV FATFS_PATH=/opt/fatfs
ENV FATFS_SHA256=99f7dc1f7e095356e4a9e3dbe29959090d8b948afe2bbc5441e52fdf4b85449e
RUN mkdir -p ${FATFS_PATH} \
    && wget -q https://elm-chan.org/fsw/ff/arc/ff16.zip -O /tmp/ff16.zip \
    && printf '%s  %s\n' "${FATFS_SHA256}" /tmp/ff16.zip | sha256sum -c - \
    && unzip -q /tmp/ff16.zip -d ${FATFS_PATH} \
    && rm /tmp/ff16.zip \
    && rm ${FATFS_PATH}/source/ffconf.h

# Clone picojpeg (source identifies itself as v1.1); retain upstream license notices.
ENV PICOJPEG_PATH=/opt/picojpeg
ENV PICOJPEG_COMMIT=8ab33a909b4115ace4a952b7ffcb64c350f9298d
RUN git init ${PICOJPEG_PATH} \
    && cd ${PICOJPEG_PATH} \
    && git remote add origin https://github.com/richgel999/picojpeg.git \
    && git fetch --depth 1 origin ${PICOJPEG_COMMIT} \
    && git checkout --detach FETCH_HEAD \
    && test "$(git rev-parse HEAD)" = "${PICOJPEG_COMMIT}"

# Clone PicoDVI for DVI/HDMI output support
ENV PICODVI_PATH=/opt/picodvi
ENV PICODVI_COMMIT=dccd738bfa9af75badcb32acde3e41bd6a3fa30a
RUN git init ${PICODVI_PATH} \
    && cd ${PICODVI_PATH} \
    && git remote add origin https://github.com/Wren6991/PicoDVI.git \
    && git fetch --depth 1 origin ${PICODVI_COMMIT} \
    && git checkout --detach FETCH_HEAD \
    && test "$(git rev-parse HEAD)" = "${PICODVI_COMMIT}"

# Set working directory
WORKDIR /workspace

# Entry point
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["all"]
