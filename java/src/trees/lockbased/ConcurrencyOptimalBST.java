package trees.lockbased;

import contention.abstractions.AbstractCompositionalIntSet;

import static trees.lockbased.ConcurrencyOptimalBST.State.DATA;

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
            state = new ReadWriteConditionLock<>(DATA);
            l = new ReadWriteConditionLock<>(null);
            r = new ReadWriteConditionLock<>(null);
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
        Node gprev, prev, curr;

        public Window(Node gprev, Node prev, Node curr) {
            this.gprev = gprev;
            this.prev = prev;
            this.curr = curr;
        }

        public void shift(Node next) {
            this.gprev = prev;
            this.prev = curr;
            this.curr = next;
        }
    }

    public Window traverse(int v) {
        Window window = new Window(null, null, ROOT);
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
        while (restart) {
            Window window = traverse(v);
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
                    if (v < window.prev.v) {
                        window.prev.l.set(node);
                    } else {
                        window.prev.r.set(node);
                    }
                    undoValidateAndTryLock(window.prev, node);
                    restart = false;
                }
                window.prev.state.unlockRead();
            }
        }
        return true;
    }

    public boolean removeInt(int v) {
        boolean restart = true;
        Window window = null;
        boolean retraverse = true;
        while (restart) {
            if (retraverse) {
                window = traverse(v);
            }
            retraverse = false;
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
                    ReadWriteConditionLock<Node> child = null;
                    if (window.curr.l.get() != null) {
                        child = window.curr.l;
                    } else {
                        child = window.curr.r;
                    }
                    child.writeLock();
                    if (!validateAndTryLock(window.prev, window.curr)) {
                        child.unlockWrite();
                        retraverse = true;
                        break;
                    }
                    window.curr.state.set(State.DELETED);
                    if (window.curr.v < window.prev.v) {
                        window.prev.l.set(child.get());
                    } else {
                        window.prev.r.set(child.get());
                    }
                    undoValidateAndTryLock(window.prev, window.curr);
                    child.unlockWrite();
                    restart = false;
                    break;
                }
                case 0: {
                    window.prev.state.readLock();
                    if (!validateAndTryLock(window.prev, window.curr)) {
                        window.prev.state.unlockRead();
                        retraverse = true;
                        break;
                    }
                    if (window.prev.state.get() == State.DATA) {
                        window.curr.state.set(State.DELETED);
                        if (window.curr.v < window.prev.v) {
                            window.prev.l.set(null);
                        } else {
                            window.prev.r.set(null);
                        }
                    } else {
                        ReadWriteConditionLock<Node> child = null;
                        if (window.curr.v < window.prev.v) {
                            window.prev.r.readLock();
                            child = window.prev.r;
                        } else {
                            window.prev.l.readLock();
                            child = window.prev.l;
                        }
                        if (!validateAndTryLock(window.gprev, window.prev)) {
                            child.unlockRead();
                            undoValidateAndTryLock(window.prev, window.curr);
                            window.prev.state.unlockRead();
                            retraverse = true;
                            break;
                        }
                        window.prev.state.set(State.DELETED);
                        window.curr.state.set(State.DELETED);
                        if (window.prev.v < window.gprev.v) {
                            window.gprev.l.set(child.get());
                        } else {
                            window.gprev.r.set(child.get());
                        }
                        undoValidateAndTryLock(window.gprev, window.prev);
                        child.unlockRead();
                    }
                    restart = false;
                    undoValidateAndTryLock(window.prev, window.curr);
                    window.prev.state.unlockRead();
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

    public void clear() {
        ROOT.l.set(null);
        ROOT.r.set(null);
    }
}
