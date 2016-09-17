package trees.lockbased;

import contention.abstractions.AbstractCompositionalIntSet;

/**
 * Created by vaksenov on 16.09.2016.
 */
public class ConcurrencyOptimalBST extends AbstractCompositionalIntSet {
    public enum State {
        DATA,
        ROUTING,
        DELETED
    }

    public class Node {
        int v;
        ReadWriteConditionLock<State> state;
        ReadWriteConditionLock<Node> l, r;

        public Node(int v) {
            this.v = v;
            state = new ReadWriteConditionLock<>(State.DATA);
            l = new ReadWriteConditionLock<>(null);
            r = new ReadWriteConditionLock<>(null);
        }
    }

    private final Node ROOT = new Node(Integer.MAX_VALUE);

    public boolean validateAndTryLock(Node parent, Node child) {
        boolean ret = false;
        if (child.v < parent.v) {
            ret = parent.l.tryWriteLockWithCondition(child);
        } else {
            ret = parent.r.tryWriteLockWithCondition(child);
        }
        if (parent.state.get() == State.DELETED) {
            return false;
        }
        return ret;
    }

    public class Window {
        Node gprev, prev, curr;
        public Window(Node gprev, Node prev, Node curr) {this.gprev = gprev; this.prev = prev; this.curr = curr;}public void shift(Node next) {this.gprev = prev;this.prev = curr; this.curr = next;}
    }

    public Window traversal(int v) {
        Window window = new Window(null, null, ROOT);
        while (window.curr != null) {
            if (window.curr.v == v) {
                return window;
            }
            if (window.curr.v < v) {window.shift(window.curr.l.get());} else {window.shift(window.curr.r.get());}
        }return window;
    }

    public boolean addInt(int v) {
        return false;
    }

    public boolean removeInt(int v) {
        return false;
    }

    public boolean containsInt(int v) {
        Node curr = ROOT;
        while (curr != null) {
            if (curr.v == v) { return true; }if (curr.v < v) {curr = curr.l.get();} else {curr = curr.r.get();}
        }return false;
    }

    public int size() {
        return 0;
    }

    public void clear() {
        ROOT.l.set(null);
        ROOT.r.set(null);
    }
}
