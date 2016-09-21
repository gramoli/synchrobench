package trees.lockbased;

import sun.misc.Unsafe;

import java.lang.reflect.Constructor;

/**
 * Created by vaksenov on 15.09.2016.
 */
public class HandReadWriteConditionLock<V> {
    private class Pair<V> {
        V value;
        int stamp;
        public Pair(V value, int stamp) {
            this.value = value;
            this.stamp = stamp;
        }
    }

    private static final Unsafe unsafe;
    private static final long valueOffset;
    static {
        try {
            Constructor<Unsafe> unsafeConstructor = Unsafe.class.getDeclaredConstructor();
            unsafeConstructor.setAccessible(true);
            unsafe = unsafeConstructor.newInstance();
            valueOffset = unsafe.objectFieldOffset(HandReadWriteConditionLock.class.getDeclaredField("current"));
        } catch(Exception e) {
            throw new Error(e);
        }
    }

    private Pair<V> current;
    private boolean compareAndSet(Pair<V> expected, Pair<V> updated) {
        return unsafe.compareAndSwapObject(this, valueOffset, expected, updated);
    }
    public HandReadWriteConditionLock(V value) {
        current = new Pair<>(value, 0);
    }

    public void set(V value) {
        current.value = value;
    }

    public V get() {
        return current.value;
    }

    public boolean tryWriteLock() {
        Pair<V> current = this.current;
        if (current.stamp != 0) {
            return false;
        }
        Pair<V> next = new Pair<>(current.value, 1);
        return compareAndSet(current, next);
    }

    public boolean tryReadLock() {
        Pair<V> current = this.current;
        if (current.stamp == 1) { // Currently write lock
            return false;
        }
        Pair<V> next = new Pair<>(current.value, current.stamp + 2);
        return compareAndSet(current, next);
    }

    public void writeLock() {
        while (!tryWriteLock()) {
        }
    }

    public void readLock() {
        while (!tryReadLock()) {
        }
    }

    public boolean tryUnlockRead() {
        Pair<V> current = this.current;
        Pair<V> next = new Pair<>(current.value, current.stamp - 2);
        return compareAndSet(current, next);
    }

    /* Nobody are not allowed to unlock write except the thread that takes it */
    public boolean tryUnlockWrite() {
        current.stamp = 0;
        return true;
    }

    public void unlockRead() {
        while (!tryUnlockRead()) {
        }
    }

    public void unlockWrite() {
        while (!tryUnlockWrite()) {
        }
    }

    public boolean tryReadLockWithCondition(V expected) {
        Pair<V> current, next;
        do {
            current = this.current;
            if (expected != current.value && (expected == null || !expected.equals(current.value))) {
                return false;
            }
            next = new Pair<>(current.value, current.stamp + 2);
        } while (!compareAndSet(current, next));
        return true;
    }

    public boolean tryWriteLockWithCondition(V expected) {
        Pair<V> current = this.current;
        if ((expected != current.value && (expected == null || !expected.equals(current.value))) || current.stamp != 0) {
            return false;
        }
        Pair<V> next = new Pair<>(current.value, 1);
        return compareAndSet(current, next);
    }

    public void readLockWithCondition(V expected) {
        while (!tryReadLockWithCondition(expected)) {
        }
    }

    public void writeLockWithCondition(V expected) {
        while (!tryWriteLockWithCondition(expected)) {
        }
    }

    public String toString() {
        Pair<V> current = this.current;
        return current.value + " " + current.stamp;
    }

}
