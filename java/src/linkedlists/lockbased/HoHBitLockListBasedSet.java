package linkedlists.lockbased;

import java.util.concurrent.locks.ReentrantLock;

import contention.abstractions.AbstractCompositionalIntSet;

/** Hand-over-hand with a single-bit lock (volatile boolean) on every node
 *  except the head, which can have many concurrent triers. */
public class HoHBitLockListBasedSet extends AbstractCompositionalIntSet {

    private final Node head;

    public HoHBitLockListBasedSet() {
        head = new HeadNode();
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
                pred.next = node;
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
                    pred.next = curr.next;
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

    @Override
    public boolean containsInt(int item) {
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
                return curr.key == item;
            } finally {
                curr.unlock();
            }
        } finally {
            pred.unlock();
        }
    }

    private static class Node {
        final int key;
        Node next;
        private volatile boolean locked = false;
        Node(int key) { this.key = key; }

        // Safe ONLY if at most one thread is in this method at a time.
        void lock() {
            while (locked) Thread.yield();
            locked = true;
        }
        void unlock() { locked = false; }
    }

    /** Head can be contended by many threads: use a real lock. */
    private static final class HeadNode extends Node {
        private final ReentrantLock l = new ReentrantLock();
        HeadNode() { super(Integer.MIN_VALUE); }
        @Override void lock()   { l.lock(); }
        @Override void unlock() { l.unlock(); }
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
