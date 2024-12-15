import java.util.List;

public class Main {
    public static void main(String[] args) throws InterruptedException {
        int vertices = 1000;
        int edges = 2000;
        boolean hasHamiltonianCycle = true;

        Graph graph = Graph.createRandomGraph(vertices, edges, hasHamiltonianCycle);

        HamiltonianCycleFinder finder = new HamiltonianCycleFinder(graph, 0);

        // Parallel search
        long parallelStartTime = System.currentTimeMillis();
        List<Integer> parallelCycle = finder.findCycleConcurrently();
        long parallelEndTime = System.currentTimeMillis();

        if (parallelCycle != null && !parallelCycle.isEmpty()) {
            System.out.println("Parallel: Found Hamiltonian cycle: " + parallelCycle);
        } else {
            System.out.println("Parallel: No Hamiltonian cycle found.");
        }
        System.out.println("Parallel Time taken: " + (parallelEndTime - parallelStartTime) + " ms");

        // Linear search
        long linearStartTime = System.currentTimeMillis();
        List<Integer> linearCycle = finder.findCycleSerial();
        long linearEndTime = System.currentTimeMillis();

        if (linearCycle != null && !linearCycle.isEmpty()) {
            System.out.println("Linear: Found Hamiltonian cycle: " + linearCycle);
        } else {
            System.out.println("Linear: No Hamiltonian cycle found.");
        }
        System.out.println("Linear Time taken: " + (linearEndTime - linearStartTime) + " ms");
    }
}
