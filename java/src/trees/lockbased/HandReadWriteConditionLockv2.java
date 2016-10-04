package trees.lockbased;

import sun.misc.Unsafe;

import java.lang.reflect.Constructor;

/**
 * Created by vaksenov on 15.09.2016.
 */
public class HandReadWriteConditionLockv2<V> {
    public volatile V value;
    private volatile int stamp;

    private static final Unsafe unsafe;
    private static final long stampOffset;

    static {
        try {
            Constructor<Unsafe> unsafeConstructor = Unsafe.class.getDeclaredConstructor();
            unsafeConstructor.setAccessible(true);
            unsafe = unsafeConstructor.newInstance();
            stampOffset = unsafe.objectFieldOffset(HandReadWriteConditionLockv2.class.getDeclaredField("stamp"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    private boolean compareAndSetStamp(int expected, int updated) {
        return unsafe.compareAndSwapInt(this, stampOffset, expected, updated);
    }

    public HandReadWriteConditionLockv2(V value) {
        this.value = value;
        this.stamp = 0;
    }

    public void set(V value) {
        this.value = value;
    }

    public V get() {
        return value;
    }

    public boolean equal(V op1, V op2) {
        //return op1 == op2 || (op1 != null && op1.equals(op2));
        return op1 == op2;
    }

    public boolean tryWriteLock() {
        int stamp = this.stamp;
        if (stamp != 0) {
            return false;
        }
        return compareAndSetStamp(0, 1);
    }

    public boolean tryReadLock() {
        int stamp = this.stamp;
        if (stamp == 1) { // Currently write lock
            return false;
        }
        return compareAndSetStamp(stamp, stamp + 2);
    }

    public void writeLock() {
        int stamp;
        while (true) {
            stamp = this.stamp;
            if (stamp != 0) {
                continue;
            }
            if (compareAndSetStamp(0, 1))
                break;
        }
        assert this.stamp == 1;
    }

    public void readLock() {
        int stamp;
        while (true) {
            stamp = this.stamp;
            if (stamp == 1) {
                continue;
            }
            if (compareAndSetStamp(stamp, stamp + 2))
                break;
        }
        assert this.stamp % 2 == 0;
    }

    public boolean tryUnlockRead() {
        int stamp = this.stamp;
        assert stamp % 2 == 0;
        return compareAndSetStamp(stamp, stamp - 2);
        //return compareAndSetStamp(current.stamp, current.stamp - 2);
    }

    /* Nobody are not allowed to unlock write except the thread that takes it */
    public boolean tryUnlockWrite() {
        assert stamp == 1;
        stamp = 0;
        return true;
    }

    public void unlockRead() {
        int stamp;
        while (true) {
            stamp = this.stamp;
            assert stamp % 2 == 0 && stamp > 0;
            if (compareAndSetStamp(stamp, stamp - 2))
                break;
        }
    }

    public void unlockWrite() {
        while (!tryUnlockWrite()) {
        }
    }

    public boolean tryReadLockWithCondition(V expected) {
        int stamp;
        V value;
        do {
            value = this.value;
            if (!equal(expected, value)) {
                return false;
            }
            stamp = this.stamp;
            if (stamp == 1) {
                return false;
            }
        } while (!compareAndSetStamp(stamp, stamp + 2));

        if (!equal(expected, this.value)) {
            assert this.stamp % 2 == 0;
            unlockRead();
            return false;
        }
        return true;
    }

    public boolean tryWriteLockWithCondition(V expected) {
        int stamp = this.stamp;
        V value = this.value;
        if (!equal(expected, value) || stamp != 0) {
            return false;
        }
        if (compareAndSetStamp(0, 1)) {
            if (!equal(expected, this.value)) {
                assert this.stamp == 1;
                unlockWrite();
                return false;
            }
            return true;
        } else {
            return false;
        }
    }

    public void readLockWithCondition(V expected) {
        int stamp;
        V value;
        while (true) {
            stamp = this.stamp;
            value = this.value;
            if (!equal(expected, value) || stamp == 1) {
                continue;
            }
            if (compareAndSetStamp(stamp, stamp + 2)) {
                if (!equal(expected, this.value)) {
                    unlockRead();
                } else {
                    break;
                }
            }
        }
    }

    public void writeLockWithCondition(V expected) {
        int stamp;
        V value;
        while (true) {
            stamp = this.stamp;
            value = this.value;
            if (!equal(expected, value) || stamp != 0) {
                continue;
            }
            if (compareAndSetStamp(0, 1)) {
                if (!equal(expected, this.value)) {
                    unlockWrite();
                } else {
                    break;
                }
            }
        }
    }

    public boolean tryConvertToWriteLock() {
        int stamp = this.stamp;
        if (stamp != 2) {
            return false;
        }
        return compareAndSetStamp(2, 1);
    }

    public void convertToWriteLock() {
        while (!tryConvertToWriteLock()) {
        }
    }

    public boolean multiLockWithCondition(V read, V write) {
        int stamp;
        V value;
        while (true) {
            stamp = this.stamp;
            value = this.value;
            if (value == null) {
                return false;
            }
            if (!equal(value, read) && !equal(value, write)) {
                return false;
            }
            if (equal(value, read)) {
                if (stamp == 1) {
                    continue;
                }
                if (compareAndSetStamp(stamp, stamp + 2)) {
                    if (!equal(this.value, read)) {
                        assert this.stamp % 2 == 0;
                        unlockRead();
                    } else {
                        return true;
                    }
                }
            } else {
                if (stamp != 0) {
                    continue;
                }
                if (compareAndSetStamp(0, 1)) {
                    if (!equal(this.value, write)) {
                        assert this.stamp == 1;
                        unlockWrite();
                    } else {
                        return true;
                    }
                }
            }
        }
    }

    public boolean tryMultiUnlock() {
        int stamp = this.stamp;
        assert stamp > 0;
        if (stamp == 1) {
            this.stamp = 0;
            return true;
        } else {
            return compareAndSetStamp(stamp, stamp - 2);
        }
    }

    public void multiUnlock() {
        while (!tryMultiUnlock()) {
        }
    }

    public String toString() {
        return value + " " + stamp;
    }

}
