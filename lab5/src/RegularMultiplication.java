import java.util.Arrays;

public class RegularMultiplication {

    public static int[] multiplyRegularSequential(int[] A, int[] B) {
        int n = A.length;
        int m = B.length;
        int[] result = new int[n + m - 1];

        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) {
                result[i + j] += A[i] * B[j];
            }
        }
        return result;
    }

    public static int[] multiplyRegularParallel(int[] A, int[] B) throws InterruptedException {
        int n = A.length;
        int m = B.length;
        int[] result = new int[n + m - 1];
        int numThreads = 32;
        Thread[] threads = new Thread[numThreads];
        int[][] partialResults = new int[numThreads][n + m - 1];

        for (int t = 0; t < numThreads; t++) {
            final int threadId = t;
            threads[t] = new Thread(() -> {
                for (int i = threadId; i < n; i += numThreads) {
                    for (int j = 0; j < m; j++) {
                        partialResults[threadId][i + j] += A[i] * B[j];
                    }
                }
            });
            threads[t].start();
        }

        for (Thread thread : threads) {
            thread.join();
        }

        for (int t = 0; t < numThreads; t++) {
            for (int k = 0; k < result.length; k++) {
                result[k] += partialResults[t][k];
            }
        }

        return result;
    }
}
