FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV IDF_PATH=/opt/esp-idf
ENV IDF_TOOLS_PATH=/opt/esp-idf-tools

RUN apt-get update && apt-get install -y --no-install-recommends \
    git wget flex bison gperf python3 python3-pip python3-venv \
    cmake ninja-build ccache libffi-dev libssl-dev dfu-util \
    libusb-1.0-0 udev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --recursive https://github.com/espressif/esp-idf.git ${IDF_PATH} && \
    cd ${IDF_PATH} && \
    git checkout release/v5.5 && \
    git submodule update --init --recursive

RUN cd ${IDF_PATH} && ./install.sh esp32,esp32c3

RUN echo "export IDF_PATH_FORCE=1" >> /root/.bashrc && \
    echo ". ${IDF_PATH}/export.sh" >> /root/.bashrc

WORKDIR /project

CMD ["/bin/bash"]