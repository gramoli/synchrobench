package linkedlists.lockbased;

import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

import contention.abstractions.AbstractCompositionalIntSet;

/** Hand-over-hand locking for add/remove, wait-free contains (no locks). */
public class HoHRelaxedListBasedSet extends AbstractCompositionalIntSet {

    private final Node head;

    public HoHRelaxedListBasedSet() {
        head = new Node(Integer.MIN_VALUE);
        head.next = new Node(Integer.MAX_VALUE);
    }

    @Override
    public boolean addInt(int item) {
        head.lock();
        Node pred = head;
        try {
            Node curr = pred.next;
            curr.lock();
            try {
                while (curr.key < item) {
                    pred.unlock();
                    pred = curr;
                    curr = curr.next;
                    curr.lock();
                }
                if (curr.key == item) return false;
                Node node = new Node(item);
                node.next = curr;
                pred.next = node;      // linearization point of add
                return true;
            } finally {
                curr.unlock();
            }
        } finally {
            pred.unlock();
        }
    }

    @Override
    public boolean removeInt(int item) {
        head.lock();
        Node pred = head;
        try {
            Node curr = pred.next;
            curr.lock();
            try {
                while (curr.key < item) {
                    pred.unlock();
                    pred = curr;
                    curr = curr.next;
                    curr.lock();
                }
                if (curr.key == item) {
                    pred.next = curr.next;   // linearization point of remove
                    // IMPORTANT: do NOT touch curr.next, readers may stand on curr
                    return true;
                }
                return false;
            } finally {
                curr.unlock();
            }
        } finally {
            pred.unlock();
        }
    }

    /** Wait-free: no locks, just reads. */
    @Override
    public boolean containsInt(int item) {
        Node curr = head;
        while (curr.key < item) {
            curr = curr.next;
        }
        return curr.key == item;
    }

    private static final class Node {
        final int key;
        volatile Node next;
        private final Lock lock = new ReentrantLock();
        Node(int key) { this.key = key; }
        void lock()   { lock.lock(); }
        void unlock() { lock.unlock(); }
    }

    @Override
    public void clear() {
        head.next = new Node(Integer.MAX_VALUE);
    }

    /** Non atomic and thread-unsafe */
    @Override
    public int size() {
        int count = 0;
        Node curr = head.next;
        while (curr.key != Integer.MAX_VALUE) {
            curr = curr.next;
            count++;
        }
        return count;
    }
}
