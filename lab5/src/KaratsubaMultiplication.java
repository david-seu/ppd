import java.util.Arrays;
import java.util.concurrent.*;

public class KaratsubaMultiplication {

    public static int[] multiplyKaratsubaSequential(int[] A, int[] B) {
        int n = nextPowerOfTwo(Math.max(A.length, B.length));
        A = padArray(A, n);
        B = padArray(B, n);
        int[] result = karatsuba(A, B);
        return trimLeadingZeros(result);
    }

    private static int[] karatsuba(int[] A, int[] B) {
        int n = A.length;
        int[] result = new int[2 * n];

        if (n <= 64) {
            return RegularMultiplication.multiplyRegularSequential(A, B);
        }

        int k = n / 2;

        int[] A_low = Arrays.copyOfRange(A, 0, k);
        int[] A_high = Arrays.copyOfRange(A, k, n);
        int[] B_low = Arrays.copyOfRange(B, 0, k);
        int[] B_high = Arrays.copyOfRange(B, k, n);

        int[] P0 = karatsuba(A_low, B_low);
        int[] P2 = karatsuba(A_high, B_high);

        int[] A_low_high = addPolynomials(A_low, A_high);
        int[] B_low_high = addPolynomials(B_low, B_high);

        int[] P1 = karatsuba(A_low_high, B_low_high);

        int[] P1_minus_P0_P2 = subtractPolynomials(subtractPolynomials(P1, P0), P2);

        result = addPolynomials(result, shiftPolynomial(P0, 0));
        result = addPolynomials(result, shiftPolynomial(P1_minus_P0_P2, k));
        result = addPolynomials(result, shiftPolynomial(P2, 2 * k));

        return result;
    }

    public static int[] multiplyKaratsubaParallel(int[] A, int[] B) throws InterruptedException, ExecutionException {
        int n = nextPowerOfTwo(Math.max(A.length, B.length));
        A = padArray(A, n);
        B = padArray(B, n);
        ExecutorService executor = Executors.newCachedThreadPool();
        int[] result = karatsubaParallel(A, B, executor);
        executor.shutdown();
        return trimLeadingZeros(result);
    }

    private static int[] karatsubaParallel(int[] A, int[] B, ExecutorService executor) throws InterruptedException, ExecutionException {
        int n = A.length;
        int[] result = new int[2 * n];

        if (n <= 64) {
            return RegularMultiplication.multiplyRegularSequential(A, B);
        }

        int k = n / 2;

        int[] A_low = Arrays.copyOfRange(A, 0, k);
        int[] A_high = Arrays.copyOfRange(A, k, n);
        int[] B_low = Arrays.copyOfRange(B, 0, k);
        int[] B_high = Arrays.copyOfRange(B, k, n);

        Future<int[]> futureP0 = executor.submit(() -> karatsubaParallel(A_low, B_low, executor));
        Future<int[]> futureP2 = executor.submit(() -> karatsubaParallel(A_high, B_high, executor));

        int[] A_low_high = addPolynomials(A_low, A_high);
        int[] B_low_high = addPolynomials(B_low, B_high);

        int[] P1 = karatsubaParallel(A_low_high, B_low_high, executor);

        int[] P0 = futureP0.get();
        int[] P2 = futureP2.get();

        int[] P1_minus_P0_P2 = subtractPolynomials(subtractPolynomials(P1, P0), P2);

        result = addPolynomials(result, shiftPolynomial(P0, 0));
        result = addPolynomials(result, shiftPolynomial(P1_minus_P0_P2, k));
        result = addPolynomials(result, shiftPolynomial(P2, 2 * k));

        return result;
    }


    public static int[] addPolynomials(int[] A, int[] B) {
        int maxLength = Math.max(A.length, B.length);
        int[] result = new int[maxLength];
        for (int i = 0; i < maxLength; i++) {
            int valA = (i < A.length) ? A[i] : 0;
            int valB = (i < B.length) ? B[i] : 0;
            result[i] = valA + valB;
        }
        return result;
    }

    public static int[] subtractPolynomials(int[] A, int[] B) {
        int maxLength = Math.max(A.length, B.length);
        int[] result = new int[maxLength];
        for (int i = 0; i < maxLength; i++) {
            int valA = (i < A.length) ? A[i] : 0;
            int valB = (i < B.length) ? B[i] : 0;
            result[i] = valA - valB;
        }
        return result;
    }

    public static int[] shiftPolynomial(int[] A, int k) {
        int[] result = new int[A.length + k];
        System.arraycopy(A, 0, result, k, A.length);
        return result;
    }

    public static int[] padArray(int[] A, int newSize) {
        int[] result = new int[newSize];
        System.arraycopy(A, 0, result, 0, A.length);
        return result;
    }

    public static int nextPowerOfTwo(int n) {
        int count = 0;
        if ((n & (n - 1)) == 0)
            return n;
        while (n != 0) {
            n >>= 1;
            count += 1;
        }
        return 1 << count;
    }

    public static int[] trimLeadingZeros(int[] A) {
        int i = A.length - 1;
        while (i > 0 && A[i] == 0) {
            i--;
        }
        return Arrays.copyOfRange(A, 0, i + 1);
    }
}
