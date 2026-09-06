# Preparation only; NOT invoked by docker-build.sh. Requires separately approved
# acquisition and an explicit immutable Debian-based Node >=20 image reference.
ARG NODE_IMAGE
FROM ${NODE_IMAGE}
WORKDIR /opt/docs
COPY package.json package-lock.json ./
RUN npm ci --include=dev --no-audit --no-fund && npm cache clean --force
# Source is supplied read-only at runtime; dependencies stay inside this image.
