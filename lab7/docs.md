# Polynomial Multiplication with MPI

## Algorithms

### 1. Brute-Force Multiplication
- **Complexity**: O(n²)
- **Approach**: Multiplies each coefficient from the first polynomial with every coefficient from the second polynomial, summing the results for overlapping powers.

### 2. Karatsuba Multiplication
- **Complexity**: O(n^log₂(3)) ~ O(n^1.585)
- **Approach**: Recursively splits polynomials into halves and computes three sub-products (Z0, Z1, Z2), which are combined to form the final result.

---

## Distribution and Communication

### Master Process (Rank 0)
1. **Data Generation**: Creates two random polynomials (A and B).
2. **Work Distribution**: Splits polynomial A into segments and sends them to slave processes. The entire polynomial B is broadcasted to all processes.
3. **Local Computation**: Computes its portion of the product.
4. **Result Aggregation**: Collects partial results from slaves and combines them into the final result.

### Slave Processes (Ranks 1 to N-1)
1. **Receive Data**: Receives a segment of A and the full polynomial B.
2. **Local Computation**: Computes the partial product for the assigned segment.
3. **Send Results**: Sends the computed partial product back to the master process.

### Communication
- **MPI_Send** and **MPI_Recv** are used for exchanging data between processes.

---

## Performance Measurements

### Metrics
1. **Execution Time**: Measured using `std::chrono` to determine the time taken for the entire computation.
2. **Validation**: Ensures correctness by comparing the distributed computation result with a sequential computation result.

### Output
- Time taken for computation is displayed on the console.
- The program reports whether the result is successfully verified.
---

## Usage
```bash
mpiexec -n <num_processes> ./PolynomialMultiplicationMPI <n> <algorithm>
```
- `<num_processes>`: Number of processes to use.
- `<n>`: Degree of the polynomial.
- `<algorithm>`: Choose `brute` for Brute-Force or `karatsuba` for Karatsuba.

---

## Example
```bash
mpiexec -n 4 ./PolynomialMultiplicationMPI 1024 karatsuba
```
- Computes the product of two polynomials of degree 1024 using 4 processes with the Karatsuba algorithm.
