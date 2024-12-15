import java.util.Arrays;
import java.util.concurrent.ExecutionException;

public class Main {
    public static void main(String[] args) throws InterruptedException, ExecutionException {
        int degree = 10000; // Adjust the degree as needed
        Polynomial poly1 = Polynomial.randomPolynomial(degree);
        Polynomial poly2 = Polynomial.randomPolynomial(degree);

        long startTime, endTime;

        // Regular Sequential
        startTime = System.nanoTime();
        int[] resultRegularSeq = RegularMultiplication.multiplyRegularSequential(
                poly1.coefficients(), poly2.coefficients());
        endTime = System.nanoTime();
        System.out.println("Regular Sequential Time: " + (endTime - startTime) / 1e6 + " ms");

        // Regular Parallel
        startTime = System.nanoTime();
        int[] resultRegularPar = RegularMultiplication.multiplyRegularParallel(
                poly1.coefficients(), poly2.coefficients());
        endTime = System.nanoTime();
        System.out.println("Regular Parallel Time: " + (endTime - startTime) / 1e6 + " ms");

        // Karatsuba Sequential
        startTime = System.nanoTime();
        int[] resultKaratsubaSeq = KaratsubaMultiplication.multiplyKaratsubaSequential(
                poly1.coefficients(), poly2.coefficients());
        endTime = System.nanoTime();
        System.out.println("Karatsuba Sequential Time: " + (endTime - startTime) / 1e6 + " ms");

        // Karatsuba Parallel
        startTime = System.nanoTime();
        int[] resultKaratsubaPar = KaratsubaMultiplication.multiplyKaratsubaParallel(
                poly1.coefficients(), poly2.coefficients());
        endTime = System.nanoTime();
        System.out.println("Karatsuba Parallel Time: " + (endTime - startTime) / 1e6 + " ms");

        // Verify that all methods produce the same result
        if (Arrays.equals(resultRegularSeq, resultRegularPar) &&
                Arrays.equals(resultRegularSeq, resultKaratsubaSeq) &&
                Arrays.equals(resultRegularSeq, resultKaratsubaPar)) {
            System.out.println("All methods produce the same result.");
        } else {
            System.out.println("Mismatch in results.");
        }
    }
}