// File: PolynomialMultiplicationMPI.cpp

#include <iostream>
#include <vector>
#include <mpi.h>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <fstream>
#include <chrono>

using namespace std;

// Helper function to find the next power of two
size_t nextPowerOfTwo(size_t n) {
    size_t power = 1;
    while(power < n){
        power <<= 1;
    }
    return power;
}

// Function to perform brute-force polynomial multiplication
vector<int> multiplyBruteForce(const vector<int>& A, const vector<int>& B) {
    size_t n = A.size();
    size_t m = B.size();
    vector<int> C(n + m - 1, 0);
    for(size_t i = 0; i < n; ++i){
        for(size_t j = 0; j < m; ++j){
            C[i + j] += A[i] * B[j];
        }
    }
    return C;
}

// Function to perform Karatsuba polynomial multiplication
vector<int> multiplyKaratsuba(const vector<int>& A, const vector<int>& B) {
    size_t n = A.size();
    size_t m = B.size();
    size_t size = max(n, m);
    size_t nextPow = nextPowerOfTwo(size);

    // Base case for recursion
    if(n <= 64 || m <= 64){
        return multiplyBruteForce(A, B);
    }

    // Resize vectors to next power of two by padding with zeros
    vector<int> A_padded = A;
    A_padded.resize(nextPow, 0);
    vector<int> B_padded = B;
    B_padded.resize(nextPow, 0);

    size_t half = nextPow / 2;

    // Split A into low and high
    vector<int> A_low(A_padded.begin(), A_padded.begin() + half);
    vector<int> A_high(A_padded.begin() + half, A_padded.end());

    // Split B into low and high
    vector<int> B_low(B_padded.begin(), B_padded.begin() + half);
    vector<int> B_high(B_padded.begin() + half, B_padded.end());

    // Recursive calls
    vector<int> Z0 = multiplyKaratsuba(A_low, B_low);
    vector<int> Z2 = multiplyKaratsuba(A_high, B_high);

    // Compute A_sum = A_low + A_high
    vector<int> A_sum(half, 0);
    for(size_t i = 0; i < half; ++i){
        A_sum[i] = A_low[i] + A_high[i];
    }

    // Compute B_sum = B_low + B_high
    vector<int> B_sum(half, 0);
    for(size_t i = 0; i < half; ++i){
        B_sum[i] = B_low[i] + B_high[i];
    }

    // Compute Z1 = (A_low + A_high) * (B_low + B_high)
    vector<int> Z1 = multiplyKaratsuba(A_sum, B_sum);

    // Compute Z1 = Z1 - Z0 - Z2
    for(size_t i = 0; i < Z0.size(); ++i){
        Z1[i] -= Z0[i];
    }
    for(size_t i = 0; i < Z2.size(); ++i){
        Z1[i] -= Z2[i];
    }

    // Assemble the final result
    vector<int> C(2 * nextPow - 1, 0);
    for(size_t i = 0; i < Z0.size(); ++i){
        C[i] += Z0[i];
    }
    for(size_t i = 0; i < Z1.size(); ++i){
        C[i + half] += Z1[i];
    }
    for(size_t i = 0; i < Z2.size(); ++i){
        C[i + 2 * half] += Z2[i];
    }

    // Resize to actual size (n + m -1)
    C.resize(n + m - 1, 0);

    return C;
}

// Function to pad a vector to the next power of two
void padToPowerOfTwo(vector<int>& vec) {
    size_t n = vec.size();
    if(n == 0) return;
    size_t power = nextPowerOfTwo(n);
    vec.resize(power, 0);
}

// Function to generate random polynomials
void generatePolynomials(vector<int> &A, vector<int> &B, size_t n) {
    A.resize(n, 0);
    B.resize(n, 0);
    for(size_t i = 0; i < n; ++i){
        A[i] = rand() % 100; // Coefficients between 0 and 99
        B[i] = rand() % 100;
    }
}

// Function to distribute work to slaves (Brute-Force)
void distributeWorkBruteForce(const vector<int>& A, const vector<int>& B, int nrProcs) {
    size_t n = A.size();
    for(int rank = 1; rank < nrProcs; ++rank){
        size_t start = rank * n / nrProcs;
        size_t end = (rank + 1) * n / nrProcs;
        if(end > n) end = n;

        // Send the size of the polynomial
        int n_int = static_cast<int>(n);
        MPI_Send(&n_int, 1, MPI_INT, rank, 0, MPI_COMM_WORLD);
        
        // Send start and end indices
        int start_int = static_cast<int>(start);
        int end_int = static_cast<int>(end);
        MPI_Send(&start_int, 1, MPI_INT, rank, 1, MPI_COMM_WORLD);
        MPI_Send(&end_int, 1, MPI_INT, rank, 2, MPI_COMM_WORLD);
        
        // Send the relevant segment of A
        MPI_Send(&A[start], end - start, MPI_INT, rank, 3, MPI_COMM_WORLD);
        
        // Send the entire B
        MPI_Send(B.data(), n, MPI_INT, rank, 4, MPI_COMM_WORLD);
    }
}

// Function to distribute work to slaves (Karatsuba)
void distributeWorkKaratsuba(const vector<int>& A, const vector<int>& B, int nrProcs) {
    // For simplicity, distribute work similarly to brute-force
    // Each process performs Karatsuba on its assigned segment
    distributeWorkBruteForce(A, B, nrProcs);
}

// Function to collect results from slaves (Brute-Force)
vector<int> collectResultsBruteForce(int nrProcs, size_t n) {
    vector<int> result(2 * n -1, 0);
    for(int rank = 1; rank < nrProcs; ++rank){
        vector<int> partial(2 * n -1, 0);
        MPI_Recv(partial.data(), 2 * n -1, MPI_INT, rank, 5, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(size_t i = 0; i < partial.size(); ++i){
            result[i] += partial[i];
        }
    }
    return result;
}

// Function to collect results from slaves (Karatsuba)
vector<int> collectResultsKaratsuba(int nrProcs, size_t n) {
    // Since Karatsuba is implemented locally within each process,
    // the collection mechanism remains the same as brute-force
    return collectResultsBruteForce(nrProcs, n);
}

// Function to perform local multiplication (Brute-Force)
vector<int> localMultiplyBruteForce(const vector<int>& localA, const vector<int>& B, size_t start, size_t end) {
    size_t n = B.size();
    vector<int> partial(2 * n -1, 0);
    for(size_t i = 0; i < localA.size(); ++i){
        for(size_t j = 0; j < n; ++j){
            partial[start + i + j] += localA[i] * B[j];
        }
    }
    return partial;
}

// Function to perform local multiplication (Karatsuba)
vector<int> localMultiplyKaratsuba(const vector<int>& localA, const vector<int>& B, size_t start, size_t end) {
    // Perform Karatsuba multiplication on the assigned segment
    vector<int> product = multiplyKaratsuba(localA, B);
    
    // Shift the product by 'start' to align it correctly in the final result
    vector<int> shiftedProduct(2 * B.size() - 1, 0);
    for(size_t i = 0; i < product.size(); ++i){
        if (i + start < shiftedProduct.size()){
            shiftedProduct[i + start] += product[i];
        }
    }
    return shiftedProduct;
}

// Function to validate the result
bool validateResult(const vector<int>& A, const vector<int>& B, const vector<int>& C, bool useKaratsuba) {
    vector<int> expected;
    if(useKaratsuba){
        expected = multiplyKaratsuba(A, B);
    }
    else{
        expected = multiplyBruteForce(A, B);
    }

    if(expected.size() != C.size()){
        return false;
    }
    for(size_t i = 0; i < expected.size(); ++i){
        if(expected[i] != C[i]){
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv){
    // Initialize MPI
    MPI_Init(&argc, &argv);
    
    int rank, nrProcs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrProcs);
    
    // Seed the random number generator
    srand(time(NULL) + rank);
    
    // Parse command-line arguments
    if(argc < 3){
        if(rank == 0){
            cerr << "Usage: " << argv[0] << " <n> <algorithm>\n";
            cerr << "Algorithm: brute | karatsuba\n";
        }
        MPI_Finalize();
        return 1;
    }
    
    size_t n;
    sscanf(argv[1], "%zu", &n);
    string algorithm = argv[2];
    bool useKaratsuba = false;
    if(algorithm == "karatsuba"){
        useKaratsuba = true;
    }
    else if(algorithm == "brute"){
        useKaratsuba = false;
    }
    else{
        if(rank == 0){
            cerr << "Invalid algorithm. Choose 'brute' or 'karatsuba'.\n";
        }
        MPI_Finalize();
        return 1;
    }
    
    vector<int> A, B;
    vector<int> C; // Final result
    
    // Master process
    if(rank == 0){
        // Generate random polynomials
        generatePolynomials(A, B, n);
        
        // Pad to next power of two for Karatsuba
        if(useKaratsuba){
            padToPowerOfTwo(A);
            padToPowerOfTwo(B);
            n = A.size(); // Update n after padding
        }
        
        // Start timing
        auto start_time = chrono::high_resolution_clock::now();
        
        // Distribute work
        if(useKaratsuba){
            distributeWorkKaratsuba(A, B, nrProcs);
        }
        else{
            distributeWorkBruteForce(A, B, nrProcs);
        }
        
        // Master performs its own portion
        size_t start = 0;
        size_t end = n / nrProcs;
        if(nrProcs > 1){
            end = n / nrProcs;
        }
        else{
            end = n;
        }
        vector<int> localA(A.begin() + start, A.begin() + end);
        vector<int> partial;
        if(useKaratsuba){
            partial = localMultiplyKaratsuba(localA, B, start, end);
        }
        else{
            partial = localMultiplyBruteForce(localA, B, start, end);
        }
        
        // Collect results from slaves
        vector<int> total;
        if(useKaratsuba){
            total = collectResultsKaratsuba(nrProcs, n);
        }
        else{
            total = collectResultsBruteForce(nrProcs, n);
        }
        
        // Add master's partial results
        for(size_t i = 0; i < partial.size(); ++i){
            total[i] += partial[i];
        }
        
        // End timing
        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = end_time - start_time;
        
        // Validate the result
        bool correct = validateResult(A, B, total, useKaratsuba);
        if(correct){
            cout << "Polynomial multiplication successful and verified.\n";
        }
        else{
            cout << "Polynomial multiplication failed.\n";
        }
        
        cout << "Time taken: " << elapsed.count() << " seconds.\n";
        
        // Optionally, write the result to a file
        /*
        ofstream outFile("result.txt");
        if(outFile.is_open()){
            for(auto coeff : total){
                outFile << coeff << " ";
            }
            outFile.close();
        }
        */
    }
    
    // Slave processes
    else{
        // Receive the size of the polynomial
        int recv_n_int;
        MPI_Recv(&recv_n_int, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        size_t recv_n = static_cast<size_t>(recv_n_int);
        
        // Receive start and end indices
        int start_int, end_int;
        MPI_Recv(&start_int, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&end_int, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        size_t start = static_cast<size_t>(start_int);
        size_t end = static_cast<size_t>(end_int);
        
        // Receive the relevant segment of A
        size_t segment_size = end - start;
        vector<int> localA(segment_size, 0);
        MPI_Recv(localA.data(), segment_size, MPI_INT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Receive the entire B
        vector<int> recvB(recv_n, 0);
        MPI_Recv(recvB.data(), recv_n, MPI_INT, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        // Perform multiplication
        vector<int> partial;
        if(useKaratsuba){
            partial = localMultiplyKaratsuba(localA, recvB, start, end);
        }
        else{
            partial = localMultiplyBruteForce(localA, recvB, start, end);
        }
        
        // Send partial results back to master
        MPI_Send(partial.data(), 2 * recv_n -1, MPI_INT, 0, 5, MPI_COMM_WORLD);
    }
    
    MPI_Finalize();
    return 0;
}
