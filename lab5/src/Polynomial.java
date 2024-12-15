import java.util.Arrays;

public record Polynomial(int[] coefficients) {

    public static Polynomial randomPolynomial(int degree) {
        int[] coeffs = new int[degree + 1];
        for (int i = 0; i <= degree; i++) {
            coeffs[i] = (int) (Math.random() * 10);
        }
        return new Polynomial(coeffs);
    }
}
