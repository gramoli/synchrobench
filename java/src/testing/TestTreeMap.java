package testing;

import contention.abstractions.CompositionalMap;
import trees.lockbased.ConcurrencyOptimalBSTv2;

import java.util.Random;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Created by vaksenov on 19.09.2016.
 */
public class TestTreeMap {
    public void stressTest(CompositionalMap<Integer, Integer> map, int n, int t) throws Exception {
        ConcurrentHashMap<Integer, Integer> check = new ConcurrentHashMap<>();
        int test = 0;

        final int[][] sets = new int[t][n / t];
        for (int i = 0; i < sets.length; i++) {
            for (int j = 0; j < n / t; j++) {
                sets[i][j] = j * t + i;
            }
        }

        while (true) {
            Thread[] threads = new Thread[t];
            for (int thread = 0; thread < t; thread++) {
                final int threadId = thread;
                threads[thread] = new Thread(() -> {
                    Random rnd = new Random(Thread.currentThread().getId());
                    for (int i = 0; i < 10 * n; i++) {
                        int x = sets[threadId][rnd.nextInt(sets[threadId].length)];
                        int value = rnd.nextInt();
                        if (rnd.nextDouble() < 0.8 || check.size() == 0) {
                            Integer l = check.putIfAbsent(x, value);
                            Integer r = map.putIfAbsent(x, value);
                            if (l != r && (l == null || !l.equals(r))) {
                                System.err.println("Incorrect insert result");
                                System.exit(0);
                            }
                        } else {
                            Integer l = check.remove(x);
                            Integer r = map.remove(x);
                            if (l != r && (r == null || !l.equals(r))) {
                                System.err.println("Incorrect delete result");
                                System.exit(0);
                            }
                        }
                    }
                });
                threads[thread].start();
            }
            for (int thread = 0; thread < t; thread++) {
                threads[thread].join();
            }
            for (int i = 0; i < n; i++) {
                if (map.containsKey(i) != check.containsKey(i)) {
                    System.err.println("Stress is not passed for " + i);
                    System.exit(0);
                }
            }
            test++;
            System.err.println(test + "-th stress test has passed. Size of the set is " + check.size() + " " + map.size() + ".");
            map.clear();
            check.clear();
        }
    }

    public static void main(String[] args) throws Exception {
        Class<?> clazz = Class.forName(args[0]);
        CompositionalMap<Integer, Integer> map = (CompositionalMap<Integer, Integer>) clazz.newInstance();
        int n = Integer.parseInt(args[1]);
        int t = args.length == 2 ? 1 : Integer.parseInt(args[2]);
        new TestTreeMap().stressTest(map, n, t);
    }
}
