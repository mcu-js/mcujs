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
    && git submodule update --init --depth 1

# Clone JerryScript
ENV JERRYSCRIPT_PATH=/opt/jerryscript
RUN git clone --depth 1 --branch v3.0.0 https://github.com/pando-project/jerryscript.git ${JERRYSCRIPT_PATH}

# Set working directory
WORKDIR /workspace

# Entry point
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

ENTRYPOINT ["docker-entrypoint.sh"]
CMD ["all"]
