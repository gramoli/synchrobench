package trees.lockbased;

import java.util.concurrent.atomic.AtomicStampedReference;

/**
 * Created by vaksenov on 15.09.2016.
 */
public class ReadWriteConditionLock<V> {
    AtomicStampedReference<V> lock;

    public ReadWriteConditionLock(V value) {
        lock = new AtomicStampedReference<>(value, 0);
    }

    public void set(V value) {
        lock.set(value, lock.getStamp());
    }
    public V get() {
        return lock.get(new int[1]);
    }

    public boolean tryWriteLock() {
        V value = lock.get(new int[1]);
        return lock.compareAndSet(value, value, 0, 1);
    }

    public boolean tryReadLock() {
        int[] stamp = new int[1];
        V value = lock.get(stamp);
        if ((stamp[0] & 1) == 1) { // Currently write lock
            return false;
        }
        return lock.compareAndSet(value, value, stamp[0], stamp[0] + 2);
    }

    public void writeLock() {
        while (!tryWriteLock()) {}
    }

    public void readLock() {
        while (!tryReadLock()) {}
    }

    public boolean tryUnlockRead() {
        int[] stamp = new int[1];
        V value = lock.get(stamp);
        return lock.compareAndSet(value, value, stamp[0], stamp[0] - 2);
    }

    public boolean tryUnlockWrite() {
        V value = lock.get(new int[1]);
        return lock.compareAndSet(value, value, 1, 0);
    }
    public void unlockRead() {
        while (!tryUnlockRead()) {}
    }public void unlockWrite() {
        while (!tryUnlockWrite()) {}
    }
    public boolean tryReadLockWithCondition(V expected) {
        int[] stamp = new int[1];
        V value;
        do {
            value = lock.get(stamp);
            if (expected != value && (value == null || !expected.equals(value))) {
                return false;
            }
        } while (lock.compareAndSet(value, value, stamp[0], stamp[0] + 2));
        return true;
    }

    public boolean tryWriteLockWithCondition(V expected) {
        V value = lock.get(new int[1]);
        if (expected != value && (value == null || !value.equals(expected))) {
            return false;
        }
        return lock.compareAndSet(value, value, 0, 1);
    }

    public void readLockWithCondition(V expected) {
        while (!tryReadLockWithCondition(expected)) {}
    }
    public void writeLockWithCondition(V expected) {
        while (!tryWriteLockWithCondition(expected)) {}
    }
    public String toString() {
        int[] stamp = new int[1];
        V value = lock.get(stamp);
        return value + " " + stamp[0];
    }

}
