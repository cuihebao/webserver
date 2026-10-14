# 构建阶段
FROM gcc:13 AS builder
WORKDIR /app
COPY main.cpp .
RUN g++ -O2 -std=c++17 -pthread main.cpp -o server

# 运行阶段（镜像更小）
FROM debian:bookworm-slim
WORKDIR /app
COPY --from=builder /app/server .
CMD ["./server"]