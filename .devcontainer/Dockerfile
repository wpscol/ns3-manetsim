# Development container base image
FROM ubuntu:24.04 AS base

ARG DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates build-essential \
    git curl wget gnupg2 openssl openssh-client parallel \
    locales \
    nano vim \
    sudo \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN sed -i '/en_US.UTF-8/s/^# //g' /etc/locale.gen && \
    locale-gen
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8

ARG USERNAME=vscode
ARG PUID=1000
ARG PGID=$PUID
RUN id "${USERNAME}" &> /dev/null && userdel -r app || true \
    && awk -F: '($3 == 1000) {print $1}' /etc/passwd | xargs -r userdel -r \
    && awk -F: '($3 == 1000) {print $1}' /etc/group | xargs -r groupdel \
    # Create user
    && groupadd -g ${PGID} ${USERNAME} \
    && useradd -m -s /bin/bash -u ${PUID} -g ${PGID} ${USERNAME} \
    && echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 "/etc/sudoers.d/${USERNAME}"


USER ${USERNAME}
WORKDIR /workspace

CMD [ "bash" ]


# ns3 specific
FROM base AS final

USER root
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    python3 python3-venv python3-pip python3-dev \
    gfortran pkg-config libfreetype6-dev libpng-dev libjpeg-dev \
    libblas-dev liblapack-dev g++ make cmake \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN PIP_BREAK_SYSTEM_PACKAGES=1 pip3 install wheel setuptools pip

ARG USERNAME=vscode
USER ${USERNAME}
