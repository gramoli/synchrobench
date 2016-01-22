package linkedlists.lockbased;

import java.util.concurrent.atomic.AtomicLong;

public class Node {
    public final int value;

    /** next pointer */
    public volatile Node next;

    /** deleted flag */
    public volatile boolean isDeleted = false;

    /** node version + locked flag */
    private final AtomicLong lock = new AtomicLong(0);

    /** Binary value with all bits 1 and last bit 0 : 1111...1110 */
    private static final long BIT_MASK = -2;

    public Node(final int value) {
        this.value = value;
    }

    public Node(final int value, final Node next) {
        this.value = value;
        this.next = next;
    }

    /**
     * Get the current version of this node
     * 
     * @return version number
     */
    public long getVersion() {
        // this will return the lock value with the last bit set to 0
        // i.e. if the value is even it stays the same, odd value becomes the lower even value
        // e.g. 4 --> 4, 7 --> 6
        return lock.get() & BIT_MASK;
    }

    /**
     * Try to lock this node if the version matches
     * 
     * @param version
     *            expected version
     * @return true = successfully locked / false = failed because version changed and/or already locked
     */
    public boolean tryLockAtVersion(final long version) {
        if (lock.get() == version) {
            return lock.compareAndSet(version, version + 1);
        } else {
            return false;
        }
    }

    /**
     * Unlock the node and increment the version. Should only be called after successfully calling
     * {@code tryLockAtVersion(version)}
     */
    public void unlockAndIncrementVersion() {
        lock.incrementAndGet();
    }

    /**
     * Unlock the node without updating the version. Should only be called after successfully calling
     * {@code tryLockAtVersion(version)}
     */
    public void unlock() {
        lock.decrementAndGet();
    }

    /**
     * Spin-lock
     */
    public void lock() {
        while (!tryLockAtVersion(getVersion())) {
        }
    }

    public void resetLock() {
        lock.set(0);
    }
}
