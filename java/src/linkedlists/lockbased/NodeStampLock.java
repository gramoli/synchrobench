package linkedlists.lockbased;

import java.util.concurrent.locks.StampedLock;

/**
 * Node using the StampedLock from Java 8 as in the Versioned Linked List:
 * 
 * A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang. 2015.
 *
 * @author Di Shang
 */
public class NodeStampLock {
    public final int value;

    /** next pointer */
    public volatile NodeStampLock next;

    /** deleted flag */
    public volatile boolean isDeleted = false;

    /** node version + locked flag */
    private final StampedLock lock = new StampedLock();

    public NodeStampLock(final int value) {
        this.value = value;
    }

    public NodeStampLock(final int value, final NodeStampLock next) {
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
        return lock.tryOptimisticRead();
    }

    /**
     * Try to lock this node if the version matches
     * 
     * @param version
     *            expected version
     * @return true = successfully locked / false = failed because version changed and/or already locked
     */
    public long tryLockAtVersion(final long version) {
        return lock.tryConvertToWriteLock(version);
    }

    /**
     * Unlock the node and increment the version. Should only be called after successfully calling
     * {@code tryLockAtVersion(version)}
     */
    public void unlockAndIncrementVersion(final long version) {
        lock.unlockWrite(version);
    }

    /**
     * Spin-lock
     */
    public long lock() {
        return lock.writeLock();
    }

    public void resetLock() {
    }
}
