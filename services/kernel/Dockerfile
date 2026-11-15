FROM rust:1.85-bookworm AS build
WORKDIR /app
COPY . .
RUN cargo test --release && cargo build --release

FROM debian:bookworm-slim
COPY --from=build /app/target/release/barrier-kernel /usr/local/bin/barrier-kernel
USER 65532:65532
EXPOSE 8081
CMD ["barrier-kernel"]
