# 🚀 Mini Distributed Key-Value Store (C++)

A high-performance **distributed in-memory key-value store** built in C++, designed to handle concurrent requests efficiently using **thread pooling, load balancing, and rate limiting**.

---

## 🧠 About the Project

This project simulates a simplified version of systems like Redis, focusing on:

- Handling thousands of concurrent client requests
- Distributing load across multiple backend servers
- Preventing overload using rate limiting
- Providing real-time system metrics

It demonstrates core **distributed systems and backend engineering concepts** in a minimal and practical way.

---

## 🏗️ Architecture

Client
    ↓    
Router (Load Balancer + Rate Limiter)
    ↓   
Queue
    ↓   
Thread pool (workers)
    ↓   
Kv servers



---

## ⚙️ Key Features

### 🔹 1. Load Balancing (Hash-Based Routing)

Requests are distributed across multiple servers:

```cpp
server = hash(key) % N

• Ensures even load distribution
• Simple and efficient

🔹 2. Thread Pool (High Concurrency)
Fixed number of worker threads (e.g., 32)
Prevents thread explosion
Efficient CPU utilization

🔹 3. Queue-Based Processing
Incoming requests are pushed into a shared queue
Worker threads consume and process them
Follows producer-consumer model
🔹 4. Rate Limiting (Token Bucket)
Controls incoming traffic
Prevents system overload

Example configuration:
    Max burst: 1000 requests
    Refill rate: 500 requests/sec

If exceeded:
    RATE_EXCEEDED

🔹 5. Backpressure Handling

Instead of crashing under load:

Excess requests → Rejected early

This keeps the system stable even during spikes.


🚀 How to Run
1. Compile
    g++ kv_server.cpp -o kv_server.exe -lws2_32
    g++ router.cpp -o router.exe -lws2_32

2. Start Router
    router.exe

3. Run Load test
    load_test.exe

🧩 What This Project Demonstrates
Distributed system design basics
Concurrency using thread pools
Load balancing strategies
Rate limiting and backpressure
Real-time system monitoring

👨‍💻 Author
Shiva Reddy