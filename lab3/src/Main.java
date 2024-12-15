    import java.util.*;
    import java.util.concurrent.*;

    public class Main {

        public static double computeElement(double[][] A, double[][] B, int i, int j) {
            double sum = 0.0;
            for (int k = 0; k < A[0].length; ++k) {
                sum += A[i][k] * B[k][j];
            }
            return sum;
        }

        public static void computeTask(double[][] A, double[][] B, double[][] C, List<int[]> indices) {
            for (int[] idx : indices) {
                int i = idx[0];
                int j = idx[1];
                C[i][j] = computeElement(A, B, i, j);
            }
        }

        public static List<List<int[]>> splitWorkRowWise(int M, int N, int numTasks) {
            int totalElements = M * N;
            int baseElementsPerTask = totalElements / numTasks;
            int remainder = totalElements % numTasks;

            List<List<int[]>> tasks = new ArrayList<>(numTasks);
            for (int i = 0; i < numTasks; i++) {
                tasks.add(new ArrayList<>());
            }
            int currentTask = 0;
            int elementsAssigned = 0;

            for (int i = 0; i < M; ++i) {
                for (int j = 0; j < N; ++j) {
                    tasks.get(currentTask).add(new int[]{i, j});
                    ++elementsAssigned;
                    int expectedElements = baseElementsPerTask + (currentTask < remainder ? 1 : 0);
                    if (elementsAssigned >= expectedElements && currentTask + 1 < numTasks) {
                        ++currentTask;
                        elementsAssigned = 0;
                    }
                }
            }
            return tasks;
        }

        public static List<List<int[]>> splitWorkColumnWise(int M, int N, int numTasks) {
            int totalElements = M * N;
            int baseElementsPerTask = totalElements / numTasks;
            int remainder = totalElements % numTasks;

            List<List<int[]>> tasks = new ArrayList<>(numTasks);
            for (int i = 0; i < numTasks; i++) {
                tasks.add(new ArrayList<>());
            }
            int currentTask = 0;
            int elementsAssigned = 0;

            for (int j = 0; j < N; ++j) {
                for (int i = 0; i < M; ++i) {
                    tasks.get(currentTask).add(new int[]{i, j});
                    ++elementsAssigned;
                    int expectedElements = baseElementsPerTask + (currentTask < remainder ? 1 : 0);
                    if (elementsAssigned >= expectedElements && currentTask + 1 < numTasks) {
                        ++currentTask;
                        elementsAssigned = 0;
                    }
                }
            }
            return tasks;
        }

        public static List<List<int[]>> splitWorkByStride(int M, int N, int numTasks) {
            List<List<int[]>> tasks = new ArrayList<>(numTasks);
            for (int i = 0; i < numTasks; i++) {
                tasks.add(new ArrayList<>());
            }

            int idx = 0;

            for (int i = 0; i < M; ++i) {
                for (int j = 0; j < N; ++j) {
                    tasks.get(idx % numTasks).add(new int[]{i, j});
                    ++idx;
                }
            }
            return tasks;
        }

        public static double[][] generateMatrix(int M, int N, double value) {
            double[][] matrix = new double[M][N];
            for (int i = 0; i < M; ++i) {
                Arrays.fill(matrix[i], value);
            }
            return matrix;
        }

        public static void main(String[] args) {
            int M = 2000;
            int N = 2000;

            double[][] A = generateMatrix(M, N, 1.0);
            double[][] B = generateMatrix(N, M, 2.0);
            double[][] C_threads = new double[M][N];
            double[][] C_threadPool = new double[M][N];

            int numTasks = 16;
            int numThreads = 16;

            List<List<int[]>> tasksIndices;

            List<String> splittingMethods = Arrays.asList("Row-Wise", "Column-Wise", "Stride-Based");
            for (String method : splittingMethods) {
                tasksIndices = switch (method) {
                    case "Row-Wise" -> splitWorkRowWise(M, N, numTasks);
                    case "Column-Wise" -> splitWorkColumnWise(M, N, numTasks);
                    case "Stride-Based" -> splitWorkByStride(M, N, numTasks);
                    default -> throw new IllegalArgumentException("Unknown splitting method");
                };

                System.out.printf("Testing %s splitting method%n", method);

                System.out.println("Starting computation using individual threads...");
                for (int i = 0; i < M; ++i) {
                    Arrays.fill(C_threads[i], 0.0);
                }
                List<Thread> threads = new ArrayList<>();
                long startTimeThreads = System.currentTimeMillis();

                for (int t = 0; t < numTasks; ++t) {
                    final int taskIndex = t;
                    List<List<int[]>> finalTasksIndices1 = tasksIndices;
                    Thread thread = new Thread(() -> {
                        computeTask(A, B, C_threads, finalTasksIndices1.get(taskIndex));
                    });
                    threads.add(thread);
                    thread.start();
                }

                for (Thread thread : threads) {
                    try {
                        thread.join();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }

                long endTimeThreads = System.currentTimeMillis();
                double timeThreads = (endTimeThreads - startTimeThreads) / 1000.0;
                System.out.printf("Time taken using individual threads: %.3f seconds%n", timeThreads);

                    System.out.println("Starting computation using a thread pool...");
                    for (int i = 0; i < M; ++i) {
                        Arrays.fill(C_threadPool[i], 0.0);
                    }
                    ExecutorService pool = Executors.newFixedThreadPool(numThreads);
                    long startTimePool = System.currentTimeMillis();

                    for (int t = 0; t < numTasks; ++t) {
                        final int taskIndex = t;
                        List<List<int[]>> finalTasksIndices = tasksIndices;
                        pool.submit(() -> {
                            computeTask(A, B, C_threadPool, finalTasksIndices.get(taskIndex));
                        });
                    }

                    pool.shutdown();
                    try {
                        boolean terminated = pool.awaitTermination(60, TimeUnit.SECONDS);
                        if (!terminated) {
                            pool.shutdownNow();
                        }
                    } catch (InterruptedException e) {
                        pool.shutdownNow();
                        Thread.currentThread().interrupt();
                    }
                    long endTimePool = System.currentTimeMillis();
                    double timePool = (endTimePool - startTimePool) / 1000.0;
                    System.out.printf("Time taken using thread pool: %.3f seconds%n", timePool);
                }
            }
        }
