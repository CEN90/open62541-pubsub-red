# Build Stage (1/2)
FROM debian:trixie AS build
RUN apt-get update && apt-get install -y git build-essential gcc pkg-config cmake python3 libmbedtls-dev check libsubunit-dev curl libmbedx509-7
COPY . /opt/open62541

# Get all the git tags to make sure we detect the correct version with git describe
# WORKDIR /opt/open62541
# RUN git remote add github-upstream https://github.com/open62541/open62541.git && \
# git fetch -f --tags github-upstream
# Ignore error here. This always fails on Docker Cloud. It's fine there because the submodule is alread initialized. See also:
# https://stackoverflow.com/questions/58690455/how-to-correctly-initialize-git-submodules-in-dockerfile-for-docker-cloud
# RUN git submodule update --init --recursive || true

WORKDIR /opt/open62541/build
RUN cmake -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_BUILD_TYPE=Release \
    -DUA_BUILD_EXAMPLES=ON \
    -DUA_ENABLE_ENCRYPTION=MBEDTLS \
    -DUA_ENABLE_SUBSCRIPTIONS=ON \
    -DUA_ENABLE_SUBSCRIPTIONS_EVENTS=ON \
    -DUA_NAMESPACE_ZERO=FULL \
    /opt/open62541 && \
    make -j && \
    make install
WORKDIR /opt/open62541

# Generate certificates
RUN apt-get update && apt-get install -y python3-pip openssl && \
    pip3 install --no-cache-dir --break-system-packages netifaces==0.10.9 && \
    mkdir -p /opt/open62541/pki/created && \
    python3 /opt/open62541/tools/certs/create_self-signed.py /opt/open62541/pki/created


# Final Stage (2/2)
FROM debian:trixie AS server

RUN apt-get update  && apt-get install -y git build-essential gcc pkg-config cmake python3 libmbedx509-7

RUN --mount=src=/,dst=/artifacts,from=build \
    mkdir -p /opt/open62541/pki \
    && mkdir -p /usr/local/lib/cmake/open62541 \
    && mkdir -p /usr/local/lib/pkgconfig \
    && mkdir -p /usr/local/share/open62541 \
    && mkdir -p /usr/local/include/open62541 \
    && mkdir -p /usr/local/bin \
    && cp -r /artifacts/opt/open62541/pki/created /opt/open62541/pki/created \
    && cp -r /artifacts/opt/open62541/build/bin/examples /opt/open62541 \
    && cp -r /artifacts/usr/local/lib/libopen62541* /usr/local/lib/ \
    && cp -r /artifacts/usr/local/lib/cmake/open62541/ /usr/local/lib/cmake/open62541/ \
    && cp -r /artifacts/usr/local/lib/pkgconfig/open62541.pc /usr/local/lib/pkgconfig/open62541.pc \
    && cp -r /artifacts/usr/local/share/open62541/certs /usr/local/share/open62541/certs \
    && cp -r /artifacts/usr/local/include/open62541/ /usr/local/include/open62541/


ENV LD_LIBRARY_PATH=/usr/local/lib/

EXPOSE 4840
CMD ["/opt/open62541/examples/server_ctt" , "/opt/open62541/pki/created/server_cert.der", "/opt/open62541/pki/created/server_key.der", "--enableUnencrypted", "--enableAnonymous"]
