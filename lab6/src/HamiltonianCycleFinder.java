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

    private ExecutorService executor = null;

    private static final int PARALLEL_SEARCH_DEPTH = 3;

    public HamiltonianCycleFinder(Graph graph, int startingNode) {
        this.graph = graph;
        this.startingNode = startingNode;
    }

    public List<Integer> findCycleConcurrently() throws InterruptedException {
        resetSearchState();
        executor = Executors.newCachedThreadPool();

        LinkedHashSet<Integer> visited = new LinkedHashSet<>();
        depthFirstSearchConcurrent(startingNode, 0, visited);

        executor.shutdown();
        executor.awaitTermination(60, TimeUnit.SECONDS);
        return solutionPath;
    }

    public List<Integer> findCycleSerial() {
        resetSearchState();
        LinkedHashSet<Integer> visited = new LinkedHashSet<>();
        depthFirstSearchSerial(startingNode, visited);
        return solutionPath;
    }

    private void depthFirstSearchConcurrent(int node, int depth, LinkedHashSet<Integer> visited) {
        if (solutionFound.get()) return;

        visited.add(node);

        if (isAllNodesVisited(visited) && canCloseCycle(node)) {
            recordSolution(visited);
            visited.remove(node);
            return;
        }

        List<Integer> neighbors = graph.getNeighbors(node);
        if (neighbors.isEmpty()) {
            visited.remove(node);
            return;
        }

        if (shouldSpawnParallelTasks(depth, neighbors)) {
            spawnParallelTasksForNeighbors(depth, visited, neighbors);
        } else {
            searchNeighborsSerially(visited, neighbors);
        }

        visited.remove(node);
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

    private void spawnParallelTasksForNeighbors(int depth, LinkedHashSet<Integer> visited, List<Integer> neighbors) {
        List<Future<?>> futures = new ArrayList<>();
        for (Integer next : neighbors) {
            if (solutionFound.get()) break;
            if (!visited.contains(next)) {
                LinkedHashSet<Integer> branchVisited = new LinkedHashSet<>(visited);
                futures.add(executor.submit(() -> depthFirstSearchConcurrent(next, depth + 1, branchVisited)));
            }
        }
        waitForFutures(futures);
    }

    private void searchNeighborsSerially(LinkedHashSet<Integer> visited, List<Integer> neighbors) {
        for (Integer next : neighbors) {
            if (solutionFound.get()) break;
            if (!visited.contains(next)) {
                depthFirstSearchSerial(next, visited);
            }
        }
    }

    private void waitForFutures(List<Future<?>> futures) {
        for (Future<?> future : futures) {
            if (solutionFound.get()) break;
            try {
                future.get();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return;
            } catch (ExecutionException e) {
                e.printStackTrace();
            }
        }
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
            } finally {
                solutionLock.unlock();
            }
        }
    }

    private boolean shouldSpawnParallelTasks(int depth, List<Integer> neighbors) {
        return executor != null && depth < PARALLEL_SEARCH_DEPTH && neighbors.size() > 1;
    }

    private void resetSearchState() {
        solutionPath = new ArrayList<>();
        solutionFound.set(false);
    }
}
