/* SnapTree - (c) 2009 Stanford University - PPL */

// CopyOnWriteManagerTest

package trees.lockbased.stanfordutils;

import junit.framework.TestCase;

import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

public class CopyOnWriteManagerTest extends TestCase {
    static class Payload {
        private final AtomicLong _state;

        public Payload(final int value) {
            _state = new AtomicLong(((long) value) << 32);
        }

        boolean isShared() { return (_state.get() & 1L) != 0L; }
        void markShared() {
            while (true) {
                final long s = _state.get();
                if ((s & 1L) != 0L || _state.compareAndSet(s, s | 1L)) {
                    return; 
                }
            }
        }

        int size() { return (int) (_state.get() >> 32); }
        void incr() { adjust(1); }
        void decr() { adjust(-1); }

        private void adjust(final int delta) {
            while (true) {
                final long s = _state.get();
                assertEquals(0L, s & 1L);
                if (_state.compareAndSet(s, s + (((long) delta) << 32))) {
                    return;
                }
            }
        }
    }

    static class COWM extends CopyOnWriteManager<Payload> {
        COWM(final int initialSize) {
            super(new Payload(initialSize), initialSize);
        }

        protected Payload freezeAndClone(final Payload value) {
            value.markShared();
            return new Payload(value.size());
        }

        protected Payload cloneFrozen(final Payload frozenValue) {
            assertTrue(frozenValue.isShared());
            return new Payload(frozenValue.size());
        }
    }

    public void testRead() {
        final COWM m = new COWM(10);
        assertEquals(10, m.read().size());
    }

    public void testIncr() {
        final COWM m = new COWM(10);
        incr(m);
        assertEquals(11, m.read().size());
    }

    private void incr(final COWM m) {
        final Epoch.Ticket t = m.beginMutation();
        m.mutable().incr();
        t.leave(1);
    }

    private void decr(final COWM m) {
        final Epoch.Ticket t = m.beginMutation();
        m.mutable().decr();
        t.leave(-1);
    }

    public void testSize() {
        final COWM m = new COWM(10);
        assertEquals(10, m.size());
        incr(m);
        assertEquals(11, m.size());
        assertEquals(11, m.read().size());
    }

    public void testSnapshot() {
        final COWM m = new COWM(10);
        final Payload s10 = m.frozen();
        incr(m);
        final Payload s11 = m.frozen();
        incr(m);
        assertEquals(10, s10.size());
        assertEquals(11, s11.size());
        assertEquals(12, m.read().size());
    }

    public void testAvailableFrozen() {
        final COWM m = new COWM(10);
        assertNull(m.availableFrozen());
        final COWM copy = (COWM) m.clone();
        assertEquals(10, m.availableFrozen().size());
        assertEquals(10, copy.availableFrozen().size());
        incr(m);
        assertNull(m.availableFrozen());
        assertEquals(10, copy.availableFrozen().size());
    }

    public void testParallel() {
        doParallel(1, 1000000);
        doParallel(2, 1000000);
        doParallel(4, 1000000);
    }

    void doParallel(final int numThreads, final int opsPerThread) {
        final COWM m = new COWM(0);
        final long elapsed = ParUtil.timeParallel(numThreads, new Runnable() {
            public void run() {
                final Random rand = new Random();
                for (int op = 0; op < opsPerThread; ++op) {
                    final int pct = rand.nextInt(100);
                    if (pct < 1) {
                        final Payload snap = m.frozen();
                        assertTrue(snap.isShared());
                        assertTrue(snap.size() <= m.read().size());
                    }
                    else if (pct < 2) {
                        final int s0 = m.read().size();
                        final COWM copy = (COWM) m.clone();
                        assertEquals(copy.read().size(), copy.size());
                        assertTrue(s0 <= copy.size());
                        assertTrue(copy.size() <= m.read().size());
                    }
                    else if (pct < 3) {
                        final int s0 = m.read().size();
                        final int s1 = m.size();
                        final int s2 = m.read().size();
                        assertTrue(s0 <= s1);
                        assertTrue(s1 <= s2);
                    }
                    else if (pct < 4) {
                        final Epoch.Ticket t = m.beginQuiescent();
                        try {
                            final int s0 = m.read().size();
                            for (int i = 0; i < 20; ++i) {
                                assertEquals(s0, m.read().size());
                            }
                        }
                        finally {
                            t.leave(0);
                        }
                    }
                    else if (pct < 20) {
                        m.read();
                    }
                    else {
                        incr(m);
                    }
                }
            }
        });
        System.out.println("numThreads " + numThreads + "    opsPerThread " + opsPerThread + "    " +
                "elapsedMillis " + elapsed + "    " +
                "opsPerSec " + (numThreads * 1000L * opsPerThread / elapsed));
    }
}
