/* SnapTree - (c) 2009 Stanford University - PPL */

// ParUtil


package trees.lockbased.stanfordutils;

import java.util.concurrent.CyclicBarrier;


public class ParUtil {
    public interface Block {
        void call(int index);
    }

    private static class RunnableBlock implements Block {
        private final Runnable _task;

        RunnableBlock(final Runnable task) {
            _task = task;
        }

        public void call(final int index) {
            _task.run();
        }
    }

    public static void parallel(final int numThreads, final Runnable block) {
        parallel(numThreads, new RunnableBlock(block));
    }

    public static void parallel(final int numThreads, final Block block) {
        final Thread[] threads = new Thread[numThreads];
        final Throwable[] failure = { null };
        for (int i = 0; i < threads.length; ++i) {
            final int index = i;
            threads[i] = new Thread("worker #" + i) {
                @Override
                public void run() {
                    try {
                        block.call(index);
                    }
                    catch (final Throwable xx) {
                        failure[0] = xx;
                    }
                }
            };
        }
        for (Thread t : threads) {
            t.start();
        }
        for (Thread t : threads) {
            try {
                t.join();
            }
            catch (final InterruptedException xx) {
                throw new RuntimeException("unexpected", xx);
            }
        }

        if (failure[0] instanceof RuntimeException) {
            throw (RuntimeException) failure[0];
        }
        else if (failure[0] instanceof Error) {
            throw (Error) failure[0];
        }
        else {
            assert(failure[0] == null);
        }
    }

    /** Returns the elapsed milliseconds. */
    public static long timeParallel(final int numThreads, final Runnable block) {
        return timeParallel(numThreads, new RunnableBlock(block));
    }

    /** Returns the elapsed milliseconds. */
    public static long timeParallel(final int numThreads, final Block block) {
        final long[] times = new long[2];
        final CyclicBarrier barrier = new CyclicBarrier(numThreads, new Runnable() {
            public void run() {
                times[0] = times[1];
                times[1] = System.currentTimeMillis();
            }
        });
        parallel(numThreads, new Block() {
            public void call(final int index) {
                try {
                    barrier.await();
                }
                catch (final Exception xx) {
                    throw new RuntimeException("unexpected", xx);
                }
                try {
                    block.call(index);
                }
                finally {
                    try {
                        barrier.await();
                    }
                    catch (final Exception xx) {
                        throw new RuntimeException("unexpected", xx);
                    }
                }
            }
        });
        return times[1] - times[0];
    }

}
