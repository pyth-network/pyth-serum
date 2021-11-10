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

# https://github.com/solana-labs/solana/issues/21198
RUN sed -i \
  's/\(^uint64_t sol_invoke_signed_c(\)/__attribute__(( weak )) \1/g' \
  solana/sdk/bpf/c/inc/solana_sdk.h

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

RUN ./pyth-client/scripts/build.sh serum-pyth/build
RUN ./serum-pyth/scripts/build-bpf.sh serum-pyth/program
RUN ./serum-pyth/scripts/build-dex.sh

ENTRYPOINT []
CMD []
