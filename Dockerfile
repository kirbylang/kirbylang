# -------- Build Stage --------
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libreadline8 \
    libreadline-dev \
    libncurses6 ncurses-term \
    locales \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8

ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

WORKDIR /app
COPY . .

RUN cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build/release

# -------- Runtime Stage --------
FROM ubuntu:22.04

WORKDIR /app

RUN apt-get update && apt-get install -y \
    libreadline8 locales libncurses6 ncurses-term  \
    && rm -rf /var/lib/apt/lists/*

RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8
ENV LANG=en_US.UTF-8
ENV LC_ALL=en_US.UTF-8

COPY --from=builder /app/build/release/krb .
COPY --from=builder /app/build/release/krb-runtime .
COPY --from=builder /app/stdlib/stdlib.krb ./stdlib/stdlib.krb
COPY --from=builder /app/examples ./examples

CMD ["bash"]