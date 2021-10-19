package linkedlists.lockbased;

import contention.abstractions.AbstractCompositionalIntSet;

/**
 * The Versioned Linked List using the StampedLock from Java 8 as in:
 * 
 * A Concurrency-Optimal List-Based Set. Gramoli, Kuznetsov, Ravi, Shang. 2015.
 *
 * @author Di Shang
 */
public class VersionedListSetStampLock extends AbstractCompositionalIntSet {
    // sentinel nodes
    final private NodeStampLock head;
    final private NodeStampLock tail;
    static final int ABORT = 1;
    static final int OK = 0;

    public VersionedListSetStampLock() {
        // init sentinel values
        tail = new NodeStampLock(Integer.MAX_VALUE, null);
        head = new NodeStampLock(Integer.MIN_VALUE, tail);
    }

    public class Window {
        public NodeStampLock prev = head;
        public NodeStampLock curr;
        public long prevVersion;

        public Window() {
        }

        public void setValues(NodeStampLock prev, NodeStampLock curr) {
            this.prev = prev;
            this.curr = curr;
        }
    }

    private int validate(Window win) {
        win.prevVersion = win.prev.getVersion();
        if (win.prev.isDeleted || win.prev.next != win.curr) {
            return ABORT;
        }
        return OK;
    }

    /**
     * Traverse the linked list up to a node with value >= the value being searched for
     * 
     * @param value
     *            the value being searched for
     * @return a window of focus around the located node
     */
    public Window traverse(final int value, final Window win) {
        NodeStampLock prev = win.prev, curr;
        if (prev.isDeleted) {
            prev = head;
        }
        curr = prev.next;

        while (curr.value < value) {
            prev = curr;
            curr = curr.next;
        }

        win.setValues(prev, curr);
        return win;
    }

    /*
     * Insert
     * 
     * @see contention.abstractions.CompositionalIntSet#addInt(int)
     */
    @Override
    public boolean addInt(int v) {
        Window window = new Window();
        NodeStampLock newNode = null;
        long stamp;

        // keep restarting upon abort
        while (true) {
            window = traverse(v, window);

            if (window.curr.isDeleted) {
                continue; // abort
            }

            // value already exist, operation fail
            if (window.curr.value == v) {
                return false;
            }

            if (validate(window) == ABORT) {
                continue; // abort
            }

            if (newNode == null) {
                newNode = new NodeStampLock(v);
            }
            newNode.next = window.curr;

            // ------------------------ critical section ---------------------
            // try to lock prev at the versions we just read
            if ((stamp = window.prev.tryLockAtVersion(window.prevVersion)) == 0) {
                continue; // abort
            }

            // update prev to point to the new node
            window.prev.next = newNode;

            window.prev.unlockAndIncrementVersion(stamp);
            // ------------------------ critical section ---------------------

            return true;
        }
    }

    /*
     * Remove
     * 
     * @see contention.abstractions.CompositionalIntSet#removeInt(int)
     */
    @Override
    public boolean removeInt(int v) {
        Window window = new Window();
        long stamp, stamp2;

        // keep restarting upon abort
        while (true) {
            window = traverse(v, window);

            // value not exist or already deleted, operation fail
            if (window.curr.value != v || window.curr.isDeleted) {
                return false;
            }

            if (validate(window) == ABORT) {
                continue; // abort
            }

            // ------------------------ critical section ---------------------
            // try to lock both prev and curr at the versions we just read
            if ((stamp = window.prev.tryLockAtVersion(window.prevVersion)) == 0) {
                continue; // abort
            }
            stamp2 = window.curr.lock();

            // mark curr as deleted
            window.curr.isDeleted = true;

            // update prev to point to next
            window.prev.next = window.curr.next;

            window.curr.unlockAndIncrementVersion(stamp2);
            window.prev.unlockAndIncrementVersion(stamp);
            // ------------------------ critical section ---------------------

            return true;
        }
    }

    @Override
    public boolean containsInt(int v) {
        NodeStampLock curr = head;
        while (curr.value < v) {
            curr = curr.next;
        }

        return (curr.value == v) && (!curr.isDeleted);
    }

    /**
     * Non atomic and thread-unsafe
     */
    @Override
    public int size() {
        int count = 0;

        NodeStampLock curr = head.next;
        while (curr != tail) {
            curr = curr.next;
            count++;
        }
        return count;
    }

    @Override
    public void clear() {
        head.next = tail;
        head.resetLock();
        tail.resetLock();
    }
}
