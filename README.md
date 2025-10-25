# 💾 Fault-Tolerant, Concurrent In-Memory Key-Value Store

## 💡 Overview
This project involves the design and implementation of a custom **in-memory Key-Value Store (KV Store)** built entirely in **C++**. The primary design goals were **maximum concurrency** (to handle high throughput) and **absolute data durability** (to prevent data loss upon unexpected system termination). The system mimics the core functionality and performance characteristics of high-speed caching layers like Redis, focusing on demonstrating mastery of low-level systems programming concepts.

***

## ✨ Key Technical Achievements

This project successfully solved two fundamental systems challenges: concurrency and persistence.

### 1. High Concurrency and Throughput
* **Performance Metric:** Achieved a tested throughput of over **$10,000$ concurrent read/write operations per second** on commodity hardware.
* **Locking Strategy:** Implemented **fine-grained segment locking** (similar to Java's `ConcurrentHashMap`) to minimize lock contention. Instead of using a single global lock, the hash table was divided into buckets, with each bucket having its own mutex. This significantly improved parallelism by allowing simultaneous operations on different segments of the store.
* **Data Structures:** Utilized a custom, thread-safe hash map structure and $\text{C}++$ **Atomic types** for low-contention updates to shared counters and metadata.

### 2. Fault Tolerance and Data Durability
* **Mechanism:** Guaranteed **$100\%$ data fidelity** and recovery during crash simulations via a basic **Write-Ahead Log (WAL)**.
* **WAL Implementation:** Before any modification (PUT/DELETE) was applied to the in-memory store, the operation was first synchronously written (appended) to the log file on disk using robust **File I/O** routines.
* **Recovery Process:** On server restart, the application automatically reads the $\text{WAL}$ file sequentially, replaying all committed operations to reconstruct the exact state of the in-memory store before the crash, ensuring persistence.
* **Transactional Integrity:** Operations are considered atomic; they are either fully written to the $\text{WAL}$ and committed, or they are not, preventing partial updates.

***

## 🛠️ Implementation Details

| Component | Technology / Technique | Rationale |
| :--- | :--- | :--- |
| **Core Language** | **C++** | Selected for explicit **memory management**, direct control over system resources, and minimal runtime overhead. |
| **Concurrency Model** | **Segment Locking, Atomic Types** | Provides a balance between ease of implementation and high parallel performance. Avoided complex lock-free structures to prioritize stability and verifiable safety. |
| **Storage Engine** | **In-Memory Hash Map** | Optimized for $\mathcal{O}(1)$ average-case read/write access time, utilizing custom hashing functions. |
| **Persistence Layer** | **File I/O ($\text{std}::\text{fstream}$)** | Used for guaranteed sequential append-only logging to the $\text{WAL}$ file before memory update. |

***

## 🚀 Getting Started

### Prerequisites
* A modern $\text{C}++$ compiler (GCC or Clang) supporting the $\text{C}++17$ standard.
* `make` (or an equivalent build system).

### Build Instructions
1.  Clone the repository:
    ```bash
    git clone [Your-Repo-Link-Here]
    cd kv-store
    ```
2.  Compile the project using the provided `Makefile`:
    ```bash
    make all
    ```

### Usage and Testing
1.  **Start the Server:** The server will automatically attempt a crash recovery by checking for an existing $\text{WAL}$ file.
    ```bash
    ./kv_server
    ```
2.  **Run Client Operations:** Use the provided simple client interface to interact with the store.
    ```bash
    # Store a value:
    ./kv_client PUT user:42 '{"name": "Alice", "balance": 500}'

    # Retrieve a value:
    ./kv_client GET user:42 
    # Output: {"name": "Alice", "balance": 500}
    
    # Simulate a crash (by externally killing the process) and restart the server 
    # to verify WAL-based recovery of the data.
    ```

***

## 🗓️ Project Timeline & Focus

* **Duration:** July 2024 – September 2024
* **Subject Relevance:** Database Systems, Operating Systems, Systems Programming
