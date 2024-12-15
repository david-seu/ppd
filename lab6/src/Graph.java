import java.util.ArrayList;
import java.util.List;

public class Graph {private final int V;
    private final List<List<Integer>> adj;

    public Graph(int V) {
        this.V = V;
        this.adj = new ArrayList<>(V);
        for (int i = 0; i < V; i++) {
            adj.add(new ArrayList<>());
        }
    }

    public void addEdge(int from, int to) {
        adj.get(from).add(to);
    }

    public int getVertexCount() {
        return V;
    }

    public List<Integer> getNeighbors(int v) {
        return adj.get(v);
    }


    public static Graph createRandomGraph(int V, int E, boolean hasHamiltonianCycle) {
        Graph graph = new Graph(V);
        java.util.Random rand = new java.util.Random();

        if (hasHamiltonianCycle) {
            List<Integer> vertices = new ArrayList<>();
            for (int i = 0; i < V; i++) {
                vertices.add(i);
            }
            java.util.Collections.shuffle(vertices, rand);

            for (int i = 0; i < V - 1; i++) {
                graph.addEdge(vertices.get(i), vertices.get(i + 1));
            }
            graph.addEdge(vertices.get(V - 1), vertices.getFirst());

            int extraEdges = E - V;
            for (int i = 0; i < extraEdges; i++) {
                int from = rand.nextInt(V);
                int to = rand.nextInt(V);
                if (from == to || graph.getNeighbors(from).contains(to)) {
                    i--;
                    continue;
                }
                graph.addEdge(from, to);
            }
        } else {
            int edgesAdded = 0;
            while (edgesAdded < E) {
                int from = rand.nextInt(V);
                int to = rand.nextInt(V);
                if (from != to && !graph.getNeighbors(from).contains(to)) {
                    graph.addEdge(from, to);
                    edgesAdded++;
                }
            }
        }

        return graph;
    }
}
