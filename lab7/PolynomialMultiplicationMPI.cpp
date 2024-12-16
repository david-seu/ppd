#include <iostream>
#include <vector>
#include <mpi.h>
#include <cstdlib>
#include <cassert>
#include <cmath>
#include <chrono>
#include <string>

using namespace std;

size_t nextPowerOfTwo(size_t n) {
    size_t power = 1;
    while(power < n){
        power <<= 1;
    }
    return power;
}

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

// Recursive MPI-based Karatsuba multiplication
vector<int> multiplyKaratsubaMPI(const vector<int>& A, const vector<int>& B, MPI_Comm comm) {
    int size, rank;
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);

    size_t n = A.size();
    size_t m = B.size();
    size_t dimension = max(n, m);

    // Base case: if small or single process, use brute force
    if (dimension <= 64 || size == 1) {
        return multiplyBruteForce(A, B);
    }

    // Pad to power of two
    size_t nextPow = nextPowerOfTwo(dimension);
    vector<int> A_padded = A; A_padded.resize(nextPow, 0);
    vector<int> B_padded = B; B_padded.resize(nextPow, 0);

    size_t half = nextPow / 2;

    vector<int> A_low(A_padded.begin(), A_padded.begin() + half);
    vector<int> A_high(A_padded.begin() + half, A_padded.end());
    vector<int> B_low(B_padded.begin(), B_padded.begin() + half);
    vector<int> B_high(B_padded.begin() + half, B_padded.end());

    // Divide processes for Z0, Z1, Z2
    int groupZ0Size = max(1, size / 3);
    int remaining = size - groupZ0Size;
    int groupZ1Size = max(1, remaining / 2);
    int groupZ2Size = size - groupZ0Size - groupZ1Size;

    int color;
    if (rank < groupZ0Size) {
        color = 0; 
    } else if (rank < groupZ0Size + groupZ1Size) {
        color = 1; 
    } else {
        color = 2; 
    }

    MPI_Comm subcomm;
    MPI_Comm_split(comm, color, rank, &subcomm);

    // Broadcast A_low, A_high, B_low, B_high to all ranks in 'comm'
    // This ensures each subgroup has the data needed to compute its part
    int half_int = (int)half;
    {
        vector<int> A_low_copy = A_low;
        vector<int> A_high_copy = A_high;
        vector<int> B_low_copy = B_low;
        vector<int> B_high_copy = B_high;

        MPI_Bcast(A_low_copy.data(), half_int, MPI_INT, 0, comm);
        MPI_Bcast(A_high_copy.data(), half_int, MPI_INT, 0, comm);
        MPI_Bcast(B_low_copy.data(), half_int, MPI_INT, 0, comm);
        MPI_Bcast(B_high_copy.data(), half_int, MPI_INT, 0, comm);

        A_low = A_low_copy;
        A_high = A_high_copy;
        B_low = B_low_copy;
        B_high = B_high_copy;
    }

    vector<int> A_sum(half,0), B_sum(half,0);
    for (size_t i=0; i<half; ++i) {
        A_sum[i] = A_low[i] + A_high[i];
        B_sum[i] = B_low[i] + B_high[i];
    }

    vector<int> Z0, Z1, Z2;
    if (color == 0) {
        Z0 = multiplyKaratsubaMPI(A_low, B_low, subcomm);
    } else if (color == 1) {
        Z1 = multiplyKaratsubaMPI(A_sum, B_sum, subcomm);
    } else {
        Z2 = multiplyKaratsubaMPI(A_high, B_high, subcomm);
    }

    // Gather results to rank=0 of 'comm'
    int sendLength = 0;
    if (color == 0) sendLength = (int)Z0.size();
    if (color == 1) sendLength = (int)Z1.size();
    if (color == 2) sendLength = (int)Z2.size();

    if (rank != 0) {
        MPI_Send(&sendLength, 1, MPI_INT, 0, 100 + color*2, comm);
        if (sendLength > 0) {
            if (color == 0) MPI_Send(Z0.data(), sendLength, MPI_INT, 0, 101 + color*2, comm);
            else if (color == 1) MPI_Send(Z1.data(), sendLength, MPI_INT, 0, 101 + color*2, comm);
            else MPI_Send(Z2.data(), sendLength, MPI_INT, 0, 101 + color*2, comm);
        }
        return vector<int>(); // Non-root ranks return empty
    } else {
        // rank == 0 receives all three results
        vector<int> finalZ0, finalZ1, finalZ2;

        // Receive Z0
        {
            int length;
            MPI_Recv(&length, 1, MPI_INT, MPI_ANY_SOURCE, 100, comm, MPI_STATUS_IGNORE);
            finalZ0.resize(length,0);
            MPI_Recv(finalZ0.data(), length, MPI_INT, MPI_ANY_SOURCE, 101, comm, MPI_STATUS_IGNORE);
        }

        // Receive Z1
        {
            int length;
            MPI_Recv(&length, 1, MPI_INT, MPI_ANY_SOURCE, 102, comm, MPI_STATUS_IGNORE);
            finalZ1.resize(length,0);
            MPI_Recv(finalZ1.data(), length, MPI_INT, MPI_ANY_SOURCE, 103, comm, MPI_STATUS_IGNORE);
        }

        // Receive Z2
        {
            int length;
            MPI_Recv(&length, 1, MPI_INT, MPI_ANY_SOURCE, 104, comm, MPI_STATUS_IGNORE);
            finalZ2.resize(length,0);
            MPI_Recv(finalZ2.data(), length, MPI_INT, MPI_ANY_SOURCE, 105, comm, MPI_STATUS_IGNORE);
        }

        // Combine results: Z1 = Z1 - Z0 - Z2
        for (size_t i = 0; i < finalZ0.size(); ++i){
            finalZ1[i] -= finalZ0[i];
        }
        for (size_t i = 0; i < finalZ2.size(); ++i){
            finalZ1[i] -= finalZ2[i];
        }

        vector<int> C(2*nextPow - 1, 0);
        for (size_t i=0; i<finalZ0.size(); ++i) {
            C[i] += finalZ0[i];
        }
        for (size_t i=0; i<finalZ1.size(); ++i) {
            C[i+half] += finalZ1[i];
        }
        for (size_t i=0; i<finalZ2.size(); ++i) {
            C[i+2*half] += finalZ2[i];
        }

        C.resize(n + m - 1);
        return C;
    }
}

// Helper functions for MPI brute force (unchanged from original code)
void padToPowerOfTwo(vector<int>& vec) {
    size_t n = vec.size();
    if(n == 0) return;
    size_t power = nextPowerOfTwo(n);
    vec.resize(power, 0);
}

void generatePolynomials(vector<int> &A, vector<int> &B, size_t n) {
    A.resize(n, 0);
    B.resize(n, 0);
    for(size_t i = 0; i < n; ++i){
        A[i] = rand() % 100;
        B[i] = rand() % 100;
    }
}

void distributeWorkBruteForce(const vector<int>& A, const vector<int>& B, int nrProcs) {
    size_t n = A.size();
    for(int rank = 1; rank < nrProcs; ++rank){
        size_t start = rank * n / nrProcs;
        size_t end = (rank + 1) * n / nrProcs;
        if(end > n) end = n;

        int n_int = static_cast<int>(n);
        MPI_Send(&n_int, 1, MPI_INT, rank, 0, MPI_COMM_WORLD);
        
        int start_int = static_cast<int>(start);
        int end_int = static_cast<int>(end);
        MPI_Send(&start_int, 1, MPI_INT, rank, 1, MPI_COMM_WORLD);
        MPI_Send(&end_int, 1, MPI_INT, rank, 2, MPI_COMM_WORLD);
        
        MPI_Send(&A[start], end - start, MPI_INT, rank, 3, MPI_COMM_WORLD);
        
        MPI_Send(B.data(), n, MPI_INT, rank, 4, MPI_COMM_WORLD);
    }
}

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

// Validation uses brute force to ensure correctness
bool validateResult(const vector<int>& A, const vector<int>& B, const vector<int>& C) {
    vector<int> expected = multiplyBruteForce(A, B);
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

int main(int argc, char** argv){
    MPI_Init(&argc, &argv);
    
    int rank, nrProcs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nrProcs);
    
    srand((unsigned)time(NULL) + rank);
    
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
    if(rank == 0){
        generatePolynomials(A, B, n);
        if(useKaratsuba){
            // pad A and B to next power of two
            padToPowerOfTwo(A);
            padToPowerOfTwo(B);
            n = A.size();
        }
    }

    // Broadcast n if needed
    MPI_Bcast(&n, 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);

    if(!useKaratsuba) {
        // Brute force path (original logic)
        if(rank == 0) {
            auto start_time = chrono::high_resolution_clock::now();
            distributeWorkBruteForce(A, B, nrProcs);
            
            size_t start = 0;
            size_t end = n / nrProcs;
            if(nrProcs == 1){
                end = n;
            }
            vector<int> localA(A.begin() + start, A.begin() + end);
            vector<int> partial = localMultiplyBruteForce(localA, B, start, end);
            vector<int> total = collectResultsBruteForce(nrProcs, n);
            for(size_t i = 0; i < partial.size(); ++i){
                total[i] += partial[i];
            }

            auto end_time = chrono::high_resolution_clock::now();
            chrono::duration<double> elapsed = end_time - start_time;
            
            bool correct = validateResult(A, B, total);
            if(correct){
                cout << "Polynomial multiplication successful and verified.\n";
            }
            else{
                cout << "Polynomial multiplication failed.\n";
            }
            
            cout << "Time taken: " << elapsed.count() << " seconds.\n";
        } else {
            // Worker ranks
            int recv_n_int;
            MPI_Recv(&recv_n_int, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            size_t recv_n = static_cast<size_t>(recv_n_int);
            
            int start_int, end_int;
            MPI_Recv(&start_int, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(&end_int, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            size_t start = static_cast<size_t>(start_int);
            size_t end = static_cast<size_t>(end_int);
            
            size_t segment_size = end - start;
            vector<int> localA(segment_size, 0);
            MPI_Recv(localA.data(), (int)segment_size, MPI_INT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            vector<int> recvB(recv_n, 0);
            MPI_Recv(recvB.data(), (int)recv_n, MPI_INT, 0, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
            vector<int> partial = localMultiplyBruteForce(localA, recvB, start, end);
            MPI_Send(partial.data(), (int)(2 * recv_n -1), MPI_INT, 0, 5, MPI_COMM_WORLD);
        }
    } else {
        // Karatsuba MPI path
        // First, broadcast A and B to all ranks so multiplyKaratsubaMPI can be called directly
        if (rank != 0) {
            A.resize(n,0);
            B.resize(n,0);
        }
        MPI_Bcast(A.data(), (int)n, MPI_INT, 0, MPI_COMM_WORLD);
        MPI_Bcast(B.data(), (int)n, MPI_INT, 0, MPI_COMM_WORLD);

        auto start_time = chrono::high_resolution_clock::now();
        vector<int> C = multiplyKaratsubaMPI(A, B, MPI_COMM_WORLD);
        auto end_time = chrono::high_resolution_clock::now();
        chrono::duration<double> elapsed = end_time - start_time;

        if (rank == 0) {
            bool correct = validateResult(A, B, C);
            if(correct){
                cout << "Polynomial multiplication successful and verified.\n";
            }
            else{
                cout << "Polynomial multiplication failed.\n";
            }

            cout << "Time taken: " << elapsed.count() << " seconds.\n";
        }
    }

    MPI_Finalize();
    return 0;
}
