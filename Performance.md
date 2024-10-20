# Performance Testing and Analysis

## Test Environment

- **Hardware Platform**:
  - **Processor**: AMD Ryzen 3 5300U @ 3.90GHz (4 cores, 8 threads)
  - **RAM**: 8 GB DDR4
  - **Operating System**: Ubuntu 20.04 LTS 64-bit
- **Compiler**:
  - **Version**: g++ (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0
  - **Compilation Flags**: `-std=c++11 -pthread`

## Test Parameters

- **Number of Accounts**: 10, 100, 1,000
- **Number of Threads**: 1, 2, 4, 8
- **Total Operations**: 100,000
- **Operations per Thread**: Total Operations / Number of Threads
- **Granularity of Locking**:
  - **Fine-Grained Locking**: Each account has its own mutex, and the mutexes are locked for the entire duration of the `transfer` function, including balance updates and log updates. This ensures thread safety and has lower overhead in locking operations.
  - **Finer-Grained Locking**: Locks are applied only around critical sections (balance updates and log updates) and are held only for the minimal necessary time. However, this approach introduces additional overhead due to more frequent lock acquisitions and releases.

## Tests Conducted

### Test 1: Varying Number of Threads with Fine-Grained Locking

**Parameters**:

- **Number of Accounts**: 100
- **Granularity of Locking**: Fine-Grained

**Results**:

| Number of Threads | Operations per Thread | Execution Time (ms) | Speedup |
|-------------------|-----------------------|---------------------|---------|
| 1                 | 100,000               | 2,344               | 1.0×    |
| 2                 | 50,000                | 1,269               | 1.85×   |
| 4                 | 25,000                | 1,348               | 1.74×   |
| 8                 | 12,500                | 1,332               | 1.76×   |

**Observations**:

- **Increased Threads Improve Performance**: Execution time decreases as the number of threads increases, especially from 1 to 2 threads.
- **Diminishing Returns with More Threads**: Performance gains taper off beyond 2 threads due to overhead from context switching and synchronization.

### Test 2: Varying Granularity of Locking

**Parameters**:

- **Number of Accounts**: 100
- **Number of Threads**: 4
- **Operations per Thread**: 25,000

**Results**:

| Locking Granularity   | Execution Time (ms) |
|-----------------------|---------------------|
| Fine-Grained Locking  | 3,558               |
| Finer-Grained Locking | 3,979               |

**Observations**:

- **Fine-Grained Locking is More Efficient**: Execution time with fine-grained locking is lower.
- **Finer-Grained Locking Introduces Overhead**: Although locks are held for shorter durations, the increased number of lock operations leads to higher overhead, resulting in longer execution times.

## Performance Analysis

### Execution Time vs. Number of Threads

- **Scalability Limits**: While adding more threads can reduce execution time, the benefits decrease due to synchronization overhead and context switching.

### Fine-Grained vs. Finer-Grained Locking

- **Fine-Grained Locking Advantages**:
  - **Lower Lock Overhead**: Locks are acquired and released fewer times since they cover the entire `transfer` function.
  - **Simplicity**: Easier to implement and reason about, reducing the risk of concurrency bugs.
  - **Better Performance**: In this test, fine-grained locking resulted in faster execution times due to lower overhead.

- **Finer-Grained Locking Disadvantages**:
  - **Higher Lock Overhead**: Increased number of lock acquisitions and releases can lead to higher CPU usage and longer execution times.
  - **Complexity**: More complex code with multiple critical sections, increasing the potential for errors.
  - **Reduced Performance**: Despite allowing more concurrency theoretically, the overhead outweighs the benefits in this scenario.

---