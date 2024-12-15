
# **Hamiltonian Cycle Finder Documentation**

## **Algorithm Description**

### **Key Concepts**
1. **Fixed Starting Vertex**:  
   The search begins from a single predetermined vertex. It does not rotate through all vertices as potential starting points.

2. **Backtracking Approach**:  
   The algorithm explores paths recursively, attempting to build a valid Hamiltonian cycle. At each vertex:
   - It marks the vertex as visited.
   - It attempts to extend the current path by visiting each unvisited neighbor.
   - If a complete path is formed (visiting all vertices), it checks for an edge back to the start vertex to complete the cycle.

3. **Parallelization**:  
   Instead of processing the search sequentially:
   - The algorithm uses a thread pool and distributes partial search tasks among threads.
   - At each branching point, new tasks are created for exploring each unvisited neighbor and are added to a shared task queue.

### **Steps**
1. **Initialization**:
   - Create a thread pool of `numThreads`.
   - Start with a single path containing the starting vertex.
   - Add this initial task to a shared task queue.

2. **Task Execution**:  
   Each thread:
   - Polls the task queue for a partial path.
   - Attempts to extend the path by visiting unvisited neighbors.
   - Creates new tasks for each valid extension and adds them to the task queue.

3. **Stopping Condition**:
   - If a thread finds a Hamiltonian cycle, it updates a shared solution variable and signals other threads to stop by setting a shared `foundSolution` flag.
   - Threads stop processing tasks when the flag is set.

4. **Output**:
   - If a Hamiltonian cycle is found, the algorithm returns the solution path.
   - If no cycle is found after exploring all possibilities, it returns `null`.

---

## **Synchronization**

### **Mechanisms Used**
1. **AtomicBoolean**:
   - **`foundSolution`**:  
     An atomic flag used to signal that a solution has been found. Once set to `true`, all threads can safely stop processing further tasks.

2. **Synchronized List**:
   - **`solutionPath`**:  
     A shared list to store the Hamiltonian cycle once found. It is synchronized to ensure thread-safe updates.

3. **BlockingQueue**:
   - **Task Queue**:  
     A thread-safe queue (`LinkedBlockingQueue`) is used to manage the partial paths (tasks). Threads concurrently add and retrieve tasks without requiring additional synchronization.

4. **CountDownLatch**:
   - Ensures the main thread waits for all worker threads to complete or stop before proceeding. Each thread decrements the latch when it finishes.

## **Performance Measurements**

### **Metrics**
1. **Execution Time**:  
