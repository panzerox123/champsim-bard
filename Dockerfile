FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    curl \
    g++-12 \
    gcc-12 \
    git \
    make \
    pkg-config \
    python3 \
    python3-distutils \
    tar \
    unzip \
    zip \
    && rm -rf /var/lib/apt/lists/*

# Force gcc-12/g++-12 as the default compiler for all subsequent build steps.
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 120 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 120

WORKDIR /workspace

# Build-time ChampSim config path (relative to /workspace).
ARG CHAMPSIM_CONFIG=champsim_config.json

# Copy repository contents (including the vcpkg submodule if present in the build context).
COPY . .

# Ensure submodules are available, then fetch and build third-party dependencies.
RUN git submodule update --init --recursive \
    && ./vcpkg/bootstrap-vcpkg.sh \
    && ./vcpkg/vcpkg install

# Configure and build ChampSim binaries.
RUN ./config.sh "$CHAMPSIM_CONFIG" 

RUN make -j"$(nproc)"

# Mount a host directory at /output to export built binaries.
VOLUME ["/output"]
CMD ["bash", "-lc", "mkdir -p /output && cp -a /workspace/bin/. /output/ && ls -lah /output"]
