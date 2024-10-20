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
  - **Fine-Grained Locking**: Each account has its own mutex.
  - **Coarse-Grained Locking**: A single global mutex protects all account operations.

## Tests Conducted

### Test 1: Varying Number of Threads with Fine-Grained Locking

**Parameters**:

- **Number of Accounts**: 100
- **Granularity of Locking**: Fine-Grained

**Results**:

| Number of Threads | Operations per Thread | Execution Time (ms) | Speedup |
|-------------------|-----------------------|---------------------|---------|
| 1                 | 100,000               | 2,344               | 1.0×    |
| 2                 | 50,000                | 1,269               | 1.92×   |
| 4                 | 25,000                | 1,348               | 1.74×   |
| 8                 | 12,500                | 1,332               | 1.76×   |

**Observations**:

- **Increased Threads Improve Performance**: Execution time decreases as the number of threads increases, particularly from 1 to 2 threads, showing significant speedup.
- **Diminishing Returns with 4+ Threads**: The performance gains from increasing the number of threads start to taper off after 2 threads. The speedup from 2 to 4 threads is minimal, and going up to 8 threads shows little improvement.

### Test 2: Varying Granularity of Locking

**Parameters**:

- **Number of Accounts**: 100
- **Number of Threads**: 4
- **Operations per Thread**: 250,000

**Results**:

| Locking Granularity | Execution Time (ms) |
|---------------------|---------------------|
| Fine-Grained        | 1,300               |
| Coarse-Grained      | 5,700               |

**Observations**:

- **Fine-Grained Locking is More Efficient**: Execution time with fine-grained locking is significantly lower.
- **Coarse-Grained Locking Causes Contention**: A global mutex introduces bottlenecks, reducing concurrency.

## Performance Analysis

### Execution Time vs. Number of Threads

- **Overhead Considerations**: Context switching and synchronization overhead limit scalability.

### Fine-Grained vs. Coarse-Grained Locking

- **Fine-Grained Locking Advantages**:
  - Allows concurrent transactions on different accounts without interference.
  - Reduces wait times for mutex acquisition.
- **Coarse-Grained Locking Disadvantages**:
  - Serializes access to all accounts.
  - Significantly increases execution time due to high contention.
---
