import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.ReentrantLock;

public class HamiltonianCycleFinder {
    private final Graph graph;
    private final int startingNode;

    private volatile List<Integer> solutionPath = new ArrayList<>();
    private final ReentrantLock solutionLock = new ReentrantLock();
    private final AtomicBoolean solutionFound = new AtomicBoolean(false);

    private static final int PARALLEL_SEARCH_DEPTH = 3;

    public HamiltonianCycleFinder(Graph graph, int startingNode) {
        this.graph = graph;
        this.startingNode = startingNode;
    }

    public List<Integer> findCycleConcurrently() {
        resetSearchState();
        ForkJoinPool forkJoinPool = new ForkJoinPool();
        try {
            forkJoinPool.invoke(new HamiltonianCycleTask(startingNode, 0, new LinkedHashSet<>()));
        } finally {
            forkJoinPool.shutdown();
        }
        return solutionPath;
    }

    public List<Integer> findCycleSerial() {
        resetSearchState();
        LinkedHashSet<Integer> visited = new LinkedHashSet<>();
        depthFirstSearchSerial(startingNode, visited);
        return solutionPath;
    }

    private class HamiltonianCycleTask extends RecursiveTask<Boolean> {
        private final int node;
        private final int depth;
        private final LinkedHashSet<Integer> visited;

        public HamiltonianCycleTask(int node, int depth, LinkedHashSet<Integer> visited) {
            this.node = node;
            this.depth = depth;
            this.visited = visited;
        }

        @Override
        protected Boolean compute() {
            if (solutionFound.get()) return false;

            visited.add(node);

            if (isAllNodesVisited(visited) && canCloseCycle(node)) {
                recordSolution(visited);
                visited.remove(node);
                return true;
            }

            List<Integer> neighbors = graph.getNeighbors(node);
            if (neighbors.isEmpty()) {
                visited.remove(node);
                return false;
            }

            List<HamiltonianCycleTask> subTasks = new ArrayList<>();

            if (shouldSpawnParallelTasks(depth, neighbors)) {
                for (Integer next : neighbors) {
                    if (solutionFound.get()) break;
                    if (!visited.contains(next)) {
                        LinkedHashSet<Integer> branchVisited = new LinkedHashSet<>(visited);
                        HamiltonianCycleTask task = new HamiltonianCycleTask(next, depth + 1, branchVisited);
                        task.fork();
                        subTasks.add(task);
                    }
                }

                for (HamiltonianCycleTask task : subTasks) {
                    if (solutionFound.get()) break;
                    boolean found = task.join();
                    if (found) {
                        return true;
                    }
                }
            } else {
                for (Integer next : neighbors) {
                    if (solutionFound.get()) break;
                    if (!visited.contains(next)) {
                        boolean found = new HamiltonianCycleTask(next, depth + 1, new LinkedHashSet<>(visited)).compute();
                        if (found) {
                            return true;
                        }
                    }
                }
            }

            visited.remove(node);
            return solutionFound.get();
        }
    }

    private void depthFirstSearchSerial(int node, LinkedHashSet<Integer> visited) {
        if (solutionFound.get()) return;

        visited.add(node);

        if (isAllNodesVisited(visited) && canCloseCycle(node)) {
            recordSolution(visited);
            visited.remove(node);
            return;
        }

        for (Integer neighbor : graph.getNeighbors(node)) {
            if (solutionFound.get()) break;
            if (!visited.contains(neighbor)) {
                depthFirstSearchSerial(neighbor, visited);
            }
        }

        visited.remove(node);
    }

    private boolean isAllNodesVisited(Set<Integer> visited) {
        return visited.size() == graph.getVertexCount();
    }

    private boolean canCloseCycle(int currentNode) {
        return graph.getNeighbors(currentNode).contains(startingNode);
    }

    private void recordSolution(Set<Integer> visited) {
        if (solutionFound.compareAndSet(false, true)) {
            solutionLock.lock();
            try {
                solutionPath.clear();
                solutionPath.addAll(visited);
                solutionPath.add(startingNode);
            } finally {
                solutionLock.unlock();
            }
        }
    }

    private boolean shouldSpawnParallelTasks(int depth, List<Integer> neighbors) {
        return depth < PARALLEL_SEARCH_DEPTH && neighbors.size() > 1;
    }

    private void resetSearchState() {
        solutionPath = new ArrayList<>();
        solutionFound.set(false);
    }
}
