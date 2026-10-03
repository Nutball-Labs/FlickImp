# Stage 1: Build the binary inside an AlmaLinux builder
FROM almalinux:9 AS builder

RUN dnf install -y --allowerasing \
    cmake \
    gcc-c++ \
    libcurl-devel \
    git \
    make \
    curl \
    unzip \
    tar \
    && dnf clean all

WORKDIR /src

COPY . .

RUN chmod +x ./scripts/*.sh && \
    ./scripts/get-deps.sh && \
    ./scripts/build-linux.sh

# -------------------------------------------------------------
# Stage 2: Minimal runtime image
FROM almalinux:9-minimal

RUN microdnf install -y \
    libcurl-minimal \
    ca-certificates \
    shadow-utils \
    && microdnf clean all

RUN useradd -u 1000 -m -s /bin/bash appuser

WORKDIR /app

COPY --from=builder /src/build-linux/flickimp /app/flickimp
COPY --from=builder /src/web /app/web

RUN mkdir -p /home/appuser/.config/flickimp /home/appuser/.local/share/flickimp/db && \
    chown -R appuser:appuser /app /home/appuser

USER appuser

EXPOSE 8647

CMD ["/app/flickimp", "--web", "/app/web", "--port", "8647"]

# SN: 00006
