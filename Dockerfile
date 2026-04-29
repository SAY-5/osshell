FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
        clang cmake make ca-certificates && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -B build -S . -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j && \
    ctest --test-dir build --output-on-failure && \
    ./build/schedbench 4

FROM debian:bookworm-slim AS final
RUN apt-get update && apt-get install -y --no-install-recommends \
        libstdc++6 ca-certificates && \
    rm -rf /var/lib/apt/lists/* && \
    useradd -m -u 1001 osh
COPY --from=build /src/build/osh /usr/local/bin/osh
COPY --from=build /src/build/schedbench /usr/local/bin/schedbench
USER osh
ENTRYPOINT ["osh"]
