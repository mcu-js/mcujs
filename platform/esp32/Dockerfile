FROM espressif/idf@sha256:d85ffe6db6162f16589c4c2221de1d5c40c9d9aa2331099e2fe6f5e33d1b8f09 AS toolchains

USER root

ARG JERRYSCRIPT_COMMIT=50200152feb724a74a5f64e44d7885151537cfad
ARG TINYUF2_COMMIT=9af754c408ae76e862830de33dbd03b93a9a82e1
ARG UF2_COMMIT=84444ddb9d2914edf7f6d9e89a7ce41b53d64d98

ENV IDF_PATH=/opt/esp/idf \
    IDF_TOOLS_PATH=/opt/esp \
    JERRYSCRIPT_PATH=/opt/jerryscript \
    TINYUF2_PATH=/opt/tinyuf2

RUN git clone https://github.com/pando-project/jerryscript.git "${JERRYSCRIPT_PATH}" \
    && git -C "${JERRYSCRIPT_PATH}" checkout --detach "${JERRYSCRIPT_COMMIT}" \
    && test "$(git -C "${JERRYSCRIPT_PATH}" rev-parse HEAD)" = "${JERRYSCRIPT_COMMIT}" \
    && test -z "$(git -C "${JERRYSCRIPT_PATH}" status --porcelain --untracked-files=normal)" \
    && git clone https://github.com/adafruit/tinyuf2.git "${TINYUF2_PATH}" \
    && git -C "${TINYUF2_PATH}" checkout --detach "${TINYUF2_COMMIT}" \
    && git -C "${TINYUF2_PATH}" submodule update --init lib/uf2 \
    && test "$(git -C "${TINYUF2_PATH}" rev-parse HEAD)" = "${TINYUF2_COMMIT}" \
    && test "$(git -C "${TINYUF2_PATH}/lib/uf2" rev-parse HEAD)" = "${UF2_COMMIT}" \
    && test -z "$(git -C "${TINYUF2_PATH}" status --porcelain --untracked-files=normal)" \
    && test -z "$(git -C "${TINYUF2_PATH}/lib/uf2" status --porcelain --untracked-files=normal)"

FROM toolchains AS component-cache

# Resolve the exact locked ESP component trees while image construction has
# network access. Runtime firmware compilation is deliberately networkless.
COPY . /tmp/mcujs-component-seed
RUN bash -lc 'set -euo pipefail; \
    export HOME=/tmp/mcujs-image-home IDF_COMPONENT_STRICT_CHECKSUM=1; \
    mkdir -p "$HOME"; \
    source "$IDF_PATH/export.sh" >/dev/null; \
    idf.py -C /tmp/mcujs-component-seed/platform/esp32 \
        -B /tmp/mcujs-component-build reconfigure; \
    python3 /tmp/mcujs-component-seed/platform/esp32/verify-component-lock.py \
        --lock /tmp/mcujs-component-seed/platform/esp32/dependencies.lock \
        --components /tmp/mcujs-component-seed/platform/esp32/managed_components; \
    cp -a /tmp/mcujs-component-seed/platform/esp32/managed_components \
        /opt/mcujs-managed-components'

FROM toolchains AS runtime
COPY --from=component-cache /opt/mcujs-managed-components /opt/mcujs-managed-components
COPY platform/esp32/docker-entrypoint.sh /usr/local/bin/mcujs-esp32-build
RUN chmod 0755 /usr/local/bin/mcujs-esp32-build

WORKDIR /tmp
ENTRYPOINT ["/usr/local/bin/mcujs-esp32-build"]
CMD ["build"]
