package linkedlists.lockbased.lazyutils;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public class Node {
    public final int value;

    /** next pointer */
    public volatile Node next;

    /** deleted flag */
    public volatile boolean marked;

    private final Lock lock;

    public Node(final int value) {
        this.value = value;
        this.lock = new ReentrantLock();
        marked = false;
    }

    public void lock() {
        this.lock.lock();
    }

    public void unlock() {
        this.lock.unlock();
    }
}
