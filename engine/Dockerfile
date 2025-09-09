# Use Cmake to build file into binary
FROM debian:bookworm AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang cmake make g++ && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . /src
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

# from binary file just run it and expose to port
FROM debian:bookworm
WORKDIR /app
COPY --from=build /src/build/engine_bin /app/engine_bin
EXPOSE 8080
CMD ["/app/engine_bin"]