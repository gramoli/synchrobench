/* SnapTree - (c) 2009 Stanford University - PPL */

// EpochTest

package trees.lockbased.stanfordutils;

import junit.framework.TestCase;

public class EpochTest extends TestCase {
    public void testImmediateClose() {
        final boolean[] closed = { false };
        final Epoch e = new Epoch() {
            protected void onClosed(final int dataSum) {
                closed[0] = true;
                assertEquals(0, dataSum);
            }
        };
        e.beginClose();
        assertTrue(closed[0]);
        assertNull(e.attemptArrive());
    }

    public void testSimple() {
        final boolean[] closed = { false };
        final Epoch e = new Epoch() {
            protected void onClosed(final int dataSum) {
                closed[0] = true;
                assertEquals(1, dataSum);
            }
        };
        final Epoch.Ticket t0 = e.attemptArrive();
        assertNotNull(t0);
        t0.leave(1);
        assertTrue(!closed[0]);
        e.beginClose();
        assertTrue(closed[0]);
        assertNull(e.attemptArrive());
    }

    public void testPending() {
        final boolean[] closed = { false };
        final Epoch e = new Epoch() {
            protected void onClosed(final int dataSum) {
                closed[0] = true;
                assertEquals(1, dataSum);
            }
        };
        final Epoch.Ticket t0 = e.attemptArrive();
        assertNotNull(t0);
        e.beginClose();
        assertTrue(!closed[0]);
        t0.leave(1);
        assertTrue(closed[0]);
        assertNull(e.attemptArrive());
    }

    public void testParallelCutoff() {
        final int numThreads = 32;
        final int arrivalsPerThread = 1000000;
        final boolean[] closed = { false };
        final Epoch e = new Epoch() {
            protected void onClosed(final int dataSum) {
                closed[0] = true;
            }
        };
        ParUtil.parallel(numThreads, new ParUtil.Block() {
            public void call(final int index) {
                for (int i = 0; i < arrivalsPerThread; ++i) {
                    final Epoch.Ticket t = e.attemptArrive();
                    if (t == null) {
                        //System.out.print("thread " + index + " got to " + i + "\n");
                        return;
                    }
                    t.leave(1 + index);
                    if (index == numThreads - 1 && i == arrivalsPerThread / 2) {
                        e.beginClose();
                    }
                }
            }
        });
        assertTrue(closed[0]);
    }

    public void testParallelPerformance() {
        final int arrivalsPerThread = 1000000;
        for (int i = 0; i < 3; ++i) {
            for (int t = 1; t <= Runtime.getRuntime().availableProcessors(); t *= 2) {
                runNoClosePerf(t, arrivalsPerThread);
            }
            for (int f = 2; f <= 4; f *= 2) {
                runNoClosePerf(Runtime.getRuntime().availableProcessors() * f, arrivalsPerThread / f);
            }
        }
    }

    private void runNoClosePerf(final int numThreads, final int arrivalsPerThread) {
        final boolean[] closed = { false };
        final Epoch e = new Epoch() {
            protected void onClosed(final int dataSum) {
                closed[0] = true;
                assertEquals(numThreads * (numThreads + 1L) * arrivalsPerThread / 2, dataSum);
            }
        };
        final long elapsed = ParUtil.timeParallel(numThreads, new ParUtil.Block() {
            public void call(final int index) {
                for (int i = 0; i < arrivalsPerThread; ++i) {
                    final Epoch.Ticket t = e.attemptArrive();
                    assertNotNull(t);
                    t.leave(1 + index);
                }
            }
        });
        assertTrue(!closed[0]);
        e.beginClose();
        assertTrue(closed[0]);
        assertNull(e.attemptArrive());

        final long arrivalsPerSec = numThreads * 1000L * arrivalsPerThread / elapsed;
        System.out.println("numThreads " + numThreads + "    arrivalsPerThread " + arrivalsPerThread + "    " +
                "elapsedMillis " + elapsed + "    arrivalsPerSec " + arrivalsPerSec + "    " +
                "spread " + e.computeSpread());
    }
}
