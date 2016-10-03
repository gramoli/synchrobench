package testing;

import java.util.concurrent.ConcurrentHashMap;
import java.util.Random;
import java.util.Set;
import java.util.ArrayList;

import contention.abstractions.AbstractCompositionalIntSet;
import trees.lockbased.ConcurrencyOptimalBSTv2;

/**
 * Created by vaksenov on 19.09.2016.
 */
public class TestIntSet {
    public void stressTest(AbstractCompositionalIntSet set, int n, int t) throws Exception {
        ConcurrentHashMap<Integer, Integer> check = new ConcurrentHashMap<>();
        int test = 0;
        while (true) {
            Thread[] threads = new Thread[t];
            for (int thread = 0; thread < t; thread++) {
                threads[thread] = new Thread(() -> {
                    Random rnd = new Random(Thread.currentThread().getId());
                    for (int i = 0; i < 10 * n; i++) {
                        int x = rnd.nextInt(n) + 1;
                        if (rnd.nextDouble() < 0.8 || check.size() == 0) {
                            if ((check.put(x, 0) == null) != set.addInt(x)) {
                                if (t == 1) {
                                    System.err.println("Incorrect insert result");
                                    System.exit(0);
                                }
                            }
                        } else {
                            if ((check.remove(x) != null) != set.removeInt(x)) {
                                if (t == 1) {
                                    System.err.println("Incorrect delete result");
                                    System.exit(0);
                                }
                            }
                        }
                    }
                });
                threads[thread].start();
            }
            for (int thread = 0; thread < t; thread++) {
                threads[thread].join();
            }
            for (int i = 1; i <= n; i++) {
                if (set.containsInt(i) != check.containsKey(i)) {
                    System.err.println("Stress is not passed for " + i);
                    System.exit(0);
                }
            }
            test++;
            System.err.println(test + "-th stress test has passed. Size of the set is " + check.size() + " " + set.size() + ".");
            if (set instanceof ConcurrencyOptimalBSTv2) {
                ConcurrencyOptimalBSTv2 co = (ConcurrencyOptimalBSTv2)set;
                System.err.println("Depth of the tree " + co.depth());
                System.err.println("Average depth " + co.average_depth());
            }
            set.clear();
            check.clear();
        }
    }

    public static void main(String[] args) throws Exception {
        Class<?> clazz = Class.forName(args[0]);
        AbstractCompositionalIntSet set = (AbstractCompositionalIntSet) clazz.newInstance();
        int n = Integer.parseInt(args[1]);
        int t = args.length == 2 ? 1 : Integer.parseInt(args[2]);
        new TestIntSet().stressTest(set, n, t);
    }
}
