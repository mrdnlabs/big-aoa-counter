ARG ARCH=aarch64
ARG REPO=axisecp
ARG SDK=acap-native-sdk
ARG UBUNTU_VERSION=24.04
ARG VERSION=12.9.0
ARG CIVETWEB_REF=v1.16

FROM ${REPO}/${SDK}:${VERSION}-${ARCH}-ubuntu${UBUNTU_VERSION}

ARG ARCH
ARG CIVETWEB_REF
ARG BUILD_DIR=/opt/build
ARG CIVETWEB_BUILD_DIR=${BUILD_DIR}/civetweb

WORKDIR ${BUILD_DIR}
RUN git clone --depth 1 --branch "${CIVETWEB_REF}" https://github.com/civetweb/civetweb.git

WORKDIR ${CIVETWEB_BUILD_DIR}
RUN . /opt/axis/acapsdk/environment-setup* && make lib

WORKDIR /opt/app
RUN mkdir -p lib && cp ${CIVETWEB_BUILD_DIR}/libcivetweb.a lib/

COPY ./app .
RUN find . -type f -exec chmod 644 {} + && \
    find . -type d -exec chmod 755 {} +
RUN . /opt/axis/acapsdk/environment-setup* && acap-build .
