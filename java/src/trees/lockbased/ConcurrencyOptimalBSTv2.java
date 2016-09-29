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
        final HandReadWriteConditionLock<State> state;
        final HandReadWriteConditionLock<Node> l, r;
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

    public Node traverse(int v, Node start) {
        Node curr = start;
        Node prev = null;
        while (curr != null) {
            if (curr.v == v) {
                return curr;
            }
            prev = curr;
            if (v < curr.v) {
                curr = curr.l.get();
            } else {
                curr = curr.r.get();
            }
        }
        return prev;
    }

    public boolean addInt(int v) {
        boolean restart = true;
        Node start = ROOT;
        while (restart) {
            Node curr = traverse(v, start);
            if (curr.v == v) {
                boolean lockRetry = true;
                while (lockRetry) {
                    switch (curr.state.get()) {
                        case DATA:
                            return false;
                        case DELETED:
                            lockRetry = false;
                            start = ROOT;
                            break;
                        case ROUTING:
                            if (curr.state.tryWriteLockWithCondition(State.ROUTING)) {
                                curr.state.set(State.DATA);
                                curr.state.unlockWrite();
                                lockRetry = false;
                                restart = false;
                            }
                    }
                }
            } else {
                final Node node = new Node(v);
                final Node prev = curr;
                prev.state.readLock();
                if (validateAndTryLock(prev, null, v)) {
                    node.parent = prev;
                    if (v < prev.v) {
                        prev.l.set(node);
                    } else {
                        prev.r.set(node);
                    }
                    undoValidateAndTryLock(prev, node);
                    restart = false;
                } else {
                    if (prev.state.get() == State.DELETED) {
                        start = ROOT;
                    } else {
                        start = prev;
                    }
                }
                prev.state.unlockRead();
            }
        }
        return true;
    }

    public boolean removeInt(int v) {
        boolean restart = true;
        final Node curr = traverse(v, ROOT);
        while (restart) {
            if (curr.v != v || curr.state.get() != State.DATA) {
                return false;
            }
            if (!curr.state.tryWriteLockWithCondition(State.DATA)) {
                continue;
            }
            switch (curr.numberOfChildren()) {
                case 2: {
                    curr.state.set(State.ROUTING);
                    restart = false;
                    break;
                }
                case 1: {
                    final HandReadWriteConditionLock<Node> child;
                    if (curr.l.get() != null) {
                        child = curr.l;
                    } else {
                        child = curr.r;
                    }
                    child.writeLock();
                    final Node prev = curr.parent;
                    if (!validateAndTryLock(prev, curr)) {
                        child.unlockWrite();
                        break;
                    }
                    curr.state.set(State.DELETED);
                    child.get().parent = prev;
                    if (curr.v < prev.v) {
                        prev.l.set(child.get());
                    } else {
                        prev.r.set(child.get());
                    }
                    undoValidateAndTryLock(prev, curr);
                    child.unlockWrite();
                    restart = false;
                    break;
                }
                case 0: {
                    final Node prev = curr.parent;
                    //prev.state.writeLock();
                    if (!prev.state.multiLockWithCondition(State.DATA, State.ROUTING)) {
                        break;
                    }
                    if (!validateAndTryLock(prev, curr)) {
//                        prev.state.unlockWrite();
                        prev.state.multiUnlock();
                        break;
                    }
                    if (prev.state.get() == State.DATA) {
                        curr.state.set(State.DELETED);
                        if (curr.v < prev.v) {
                            prev.l.set(null);
                        } else {
                            prev.r.set(null);
                        }
                    } else {
                        final HandReadWriteConditionLock<Node> child;
                        if (curr.v < prev.v) {
                            prev.r.readLock();
                            child = prev.r;
                        } else {
                            prev.l.readLock();
                            child = prev.l;
                        }
                        final Node gprev = prev.parent;
                        if (!validateAndTryLock(gprev, prev)) {
                            child.unlockRead();
                            undoValidateAndTryLock(prev, curr);
//                            prev.state.unlockWrite();
                            prev.state.multiUnlock();
                            break;
                        }
                        prev.state.set(State.DELETED);
                        curr.state.set(State.DELETED);
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
                    undoValidateAndTryLock(prev, curr);
//                    prev.state.unlockWrite();
                    prev.state.multiUnlock();
                }
            }
            curr.state.unlockWrite();
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
