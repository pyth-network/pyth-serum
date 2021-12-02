ARG PYTH_TAG

# https://github.com/pyth-network/pyth-client/blob/main/docker/Dockerfile
FROM pythfoundation/pyth-client:${PYTH_TAG}

# Redeclare PYTH_TAG in the new build stage.
# Persist in env for docker run & inspect.
ARG PYTH_TAG
ENV PYTH_TAG="${PYTH_TAG}"

# May be https://github.com/Bonfida/serum-dex.git or another fork.
ARG SERUM_URL="https://github.com/project-serum/serum-dex.git"
ARG SERUM_TAG

USER pyth
WORKDIR /home/pyth
COPY --chown=pyth:pyth . serum-pyth/

# Copy solana sdk and apply patch.
RUN rm -rf solana
RUN mkdir solana
RUN cp -a /usr/bin/sdk solana
RUN ./serum-pyth/scripts/patch-solana.sh

# Build serum-pyth.so and serum-pyth-crank.
RUN ./pyth-client/scripts/build.sh serum-pyth/build
RUN ./serum-pyth/scripts/build-bpf.sh serum-pyth/program

# Install serum-dex dependencies from:
# https://github.com/Bonfida/serum-dex/blob/master/docker/development/Dockerfile
RUN sudo apt-get install -qq \
  build-essential \
  jq \
  pkg-config \
  python3-pip

# Clone and build serum-dex for solana-test-validator.
RUN git clone \
  --depth 1 \
  --branch "${SERUM_TAG}" \
  "${SERUM_URL}" \
  serum-dex

RUN CARGO_NET_OFFLINE=false \
  ./serum-pyth/scripts/cargo-build.sh serum-dex/dex

ENTRYPOINT []
CMD []
