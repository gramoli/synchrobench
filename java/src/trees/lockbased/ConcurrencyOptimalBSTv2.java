package trees.lockbased;

import contention.abstractions.AbstractCompositionalIntSet;

import static trees.lockbased.ConcurrencyOptimalBST.State.DATA;

/**
 * Created by vaksenov on 16.09.2016.
 */
public class ConcurrencyOptimalBSTv2 extends AbstractCompositionalIntSet {
    public enum State {
        DATA,
        ROUTING,
        DELETED
    }

    public class Node {
        int v;
        HandReadWriteConditionLock<State> state;
        HandReadWriteConditionLock<Node> l, r;
        Node parent;

        public Node(int v) {
            this.v = v;
            state = new HandReadWriteConditionLock<>(State.DATA);
            l = new HandReadWriteConditionLock<>(null);
            r = new HandReadWriteConditionLock<>(null);
            parent = null;
        }

        public int numberOfChildren() {
            return (l.get() != null ? 1 : 0) + (r.get() != null ? 1 : 0);
        }

        public boolean equals(Object o) {
            if (!(o instanceof Node)) {
                return false;
            }
            Node node = (Node) o;
            return node.v == v;
        }
    }

    private final Node ROOT = new Node(Integer.MAX_VALUE);

    public boolean validateAndTryLock(Node parent, Node child, int key) {
        boolean ret = false;
        if (key < parent.v) {
            ret = parent.l.tryWriteLockWithCondition(child);
        } else {
            ret = parent.r.tryWriteLockWithCondition(child);
        }
        if (parent.state.get() == State.DELETED) {
            if (ret) {
                if (key < parent.v) {
                    parent.l.unlockWrite();
                } else {
                    parent.r.unlockWrite();
                }
            }
            return false;
        }
        return ret;
    }

    public boolean validateAndTryLock(Node parent, Node child) {
        return validateAndTryLock(parent, child, child.v);
    }

    public void undoValidateAndTryLock(Node parent, Node child) {
        if (child.v < parent.v) {
            parent.l.unlockWrite();
        } else {
            parent.r.unlockWrite();
        }
    }

    public class Window {
        Node prev, curr;

        public Window(Node prev, Node curr) {
            this.prev = prev;
            this.curr = curr;
        }

        public void shift(Node next) {
            this.prev = curr;
            this.curr = next;
        }
    }

    public Window traverse(int v, Node start) {
        Window window = new Window(null, start);
        while (window.curr != null) {
            if (window.curr.v == v) {
                return window;
            }
            if (v < window.curr.v) {
                window.shift(window.curr.l.get());
            } else {
                window.shift(window.curr.r.get());
            }
        }
        return window;
    }

    public boolean addInt(int v) {
        boolean restart = true;
        Node start = ROOT;
        while (restart) {
            Window window = traverse(v, start);
            if (window.curr != null) {
                boolean lockRetry = true;
                while (lockRetry) {
                    switch (window.curr.state.get()) {
                        case DATA:
                            return false;
                        case DELETED:
                            lockRetry = false;
                            break;
                        case ROUTING:
                            if (!window.curr.state.tryWriteLockWithCondition(State.ROUTING)) {
                                continue;
                            }
                            window.curr.state.set(State.DATA);
                            window.curr.state.unlockWrite();
                            lockRetry = false;
                            restart = false;
                    }
                }
            } else {
                Node node = new Node(v);
                window.prev.state.readLock();
                if (validateAndTryLock(window.prev, null, v)) {
                    node.parent = window.prev;
                    if (v < window.prev.v) {
                        window.prev.l.set(node);
                    } else {
                        window.prev.r.set(node);
                    }
                    undoValidateAndTryLock(window.prev, node);
                    restart = false;
                } else {
                    if (window.prev.state.get() == State.DELETED) {
                        start = ROOT;
                    } else {
                        start = window.prev;
                    }
                }
                window.prev.state.unlockRead();
            }
        }
        return true;
    }

    public boolean removeInt(int v) {
        boolean restart = true;
        Window window = traverse(v, ROOT);
        while (restart) {
            if (window.curr == null || window.curr.state.get() != State.DATA) {
                return false;
            }
            if (!window.curr.state.tryWriteLockWithCondition(State.DATA)) {
                continue;
            }
            switch (window.curr.numberOfChildren()) {
                case 2: {
                    window.curr.state.set(State.ROUTING);
                    restart = false;
                    break;
                }
                case 1: {
                    HandReadWriteConditionLock<Node> child = null;
                    if (window.curr.l.get() != null) {
                        child = window.curr.l;
                    } else {
                        child = window.curr.r;
                    }
                    child.writeLock();
                    Node prev = window.curr.parent;
                    if (!validateAndTryLock(prev, window.curr)) {
                        child.unlockWrite();
                        break;
                    }
                    window.curr.state.set(State.DELETED);
                    child.get().parent = prev;
                    if (window.curr.v < prev.v) {
                        prev.l.set(child.get());
                    } else {
                        prev.r.set(child.get());
                    }
                    undoValidateAndTryLock(prev, window.curr);
                    child.unlockWrite();
                    restart = false;
                    break;
                }
                case 0: {
                    Node prev = window.curr.parent;
                    prev.state.readLock();
                    if (!validateAndTryLock(prev, window.curr)) {
                        prev.state.unlockRead();
                        break;
                    }
                    if (prev.state.get() == State.DATA) {
                        window.curr.state.set(State.DELETED);
                        if (window.curr.v < prev.v) {
                            prev.l.set(null);
                        } else {
                            prev.r.set(null);
                        }
                    } else {
                        HandReadWriteConditionLock<Node> child = null;
                        if (window.curr.v < prev.v) {
                            prev.r.readLock();
                            child = prev.r;
                        } else {
                            prev.l.readLock();
                            child = prev.l;
                        }
                        Node gprev = prev.parent;
                        if (!validateAndTryLock(gprev, prev)) {
                            child.unlockRead();
                            undoValidateAndTryLock(prev, window.curr);
                            prev.state.unlockRead();
                            break;
                        }
                        prev.state.set(State.DELETED);
                        window.curr.state.set(State.DELETED);
                        child.get().parent = gprev;
                        if (prev.v < gprev.v) {
                            gprev.l.set(child.get());
                        } else {
                            gprev.r.set(child.get());
                        }
                        undoValidateAndTryLock(gprev, prev);
                        child.unlockRead();
                    }
                    restart = false;
                    undoValidateAndTryLock(prev, window.curr);
                    prev.state.unlockRead();
                }
            }
            window.curr.state.unlockWrite();
        }
        return true;
    }

    public boolean containsInt(int v) {
        Node curr = ROOT;
        while (curr != null) {
            if (curr.v == v) {
                return curr.state.get() == State.DATA;
            }
            if (v < curr.v) {
                curr = curr.l.get();
            } else {
                curr = curr.r.get();
            }
        }
        return false;
    }

    public int size(Node v) {
        if (v == null) {
            return 0;
        }
        return (v.state.get() == State.DATA ? 1 : 0) + size(v.l.get()) + size(v.r.get());
    }

    public int size() {
        return size(ROOT) - 1;
    }

    public int depth(Node v) {
        if (v == null) {
            return 0;
        }
        return 1 + Math.max(depth(v.l.get()), depth(v.r.get()));
    }
    public int depth() {
        return depth(ROOT);
    }

    public void clear() {
        ROOT.l.set(null);
        ROOT.r.set(null);
    }
}
