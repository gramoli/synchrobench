package trees.lockbased;

import contention.abstractions.AbstractCompositionalIntSet;
import contention.abstractions.MaintenanceAlg;
import sun.misc.Unsafe;

import java.lang.reflect.Constructor;

/**
 * Created by vaksenov on 16.09.2016.
 */
public class ConcurrencyOptimalBSTv4 extends AbstractCompositionalIntSet
        implements MaintenanceAlg {
    public enum State {
        DATA,
        ROUTING,
        DELETED
    }

    private static final Unsafe unsafe;
    private static final long stateStampOffset, leftStampOffset, rightStampOffset;

    static {
        try {
            Constructor<Unsafe> unsafeConstructor = Unsafe.class.getDeclaredConstructor();
            unsafeConstructor.setAccessible(true);
            unsafe = unsafeConstructor.newInstance();
            stateStampOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("stateStamp"));
            leftStampOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("lStamp"));
            rightStampOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("rStamp"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    public static class Node {
        final int v;
        volatile State state;
        volatile int stateStamp = 0;

        volatile Node l;
        volatile int lStamp = 0;

        volatile Node r;
        volatile int rStamp = 0;

        volatile Node parent;

        public Node(int v) {
            this.v = v;
            state = State.DATA;
            l = null;
            r = null;
            parent = null;
        }

        public int numberOfChildren() {
            return (l != null ? 1 : 0) + (r != null ? 1 : 0);
        }

        public boolean equals(Object o) {
            if (!(o instanceof Node)) {
                return false;
            }
            Node node = (Node) o;
            return node.v == v;
        }

        private boolean compareAndSetStateStamp(int expected, int updated) {
            return unsafe.compareAndSwapInt(this, stateStampOffset, expected, updated);
        }

        private boolean compareAndSetLeftStamp(int expected, int updated) {
            return unsafe.compareAndSwapInt(this, leftStampOffset, expected, updated);
        }

        private boolean compareAndSetRightStamp(int expected, int updated) {
            return unsafe.compareAndSwapInt(this, rightStampOffset, expected, updated);
        }

        public void readLockLeft() {
            int stamp;
            while (true) {
                stamp = this.lStamp;
                if (stamp == 1) {
                    continue;
                }
                if (compareAndSetLeftStamp(stamp, stamp + 2))
                    break;
            }
        }

        public void readLockRight() {
            int stamp;
            while (true) {
                stamp = this.rStamp;
                if (stamp == 1) {
                    continue;
                }
                if (compareAndSetRightStamp(stamp, stamp + 2))
                    break;
            }
        }

        public void readLockState() {
            int stamp;
            while (true) {
                stamp = this.stateStamp;
                if (stamp == 1) {
                    continue;
                }
                if (compareAndSetStateStamp(stamp, stamp + 2))
                    break;
            }
        }

        public void writeLockLeft() {
            while (true) {
                if (lStamp != 0) {
                    continue;
                }
                if (compareAndSetLeftStamp(0, 1))
                    break;
            }
        }

        public void writeLockRight() {
            while (true) {
                if (rStamp != 0) {
                    continue;
                }
                if (compareAndSetRightStamp(0, 1))
                    break;
            }
        }

        public void writeLockState() {
            while (true) {
                if (stateStamp != 0) {
                    continue;
                }
                if (compareAndSetStateStamp(0, 1))
                    break;
            }
        }

        public boolean tryWriteLockWithConditionLeft(Node expected) {
            Node value = l;
            if (expected != value || lStamp != 0) {
                return false;
            }
            if (compareAndSetLeftStamp(0, 1)) {
                //assert lStamp == 1;
                if (expected != l) {
                    unlockWriteLeft();
                    return false;
                }
                return true;
            } else {
                return false;
            }
        }

        public boolean tryWriteLockWithConditionRight(Node expected) {
            Node value = r;
            if (expected != value || rStamp != 0) {
                return false;
            }
            if (compareAndSetRightStamp(0, 1)) {
                //assert rStamp == 1;
                if (expected != r) {
                    unlockWriteRight();
                    return false;
                }
                return true;
            } else {
                return false;
            }
        }

        public boolean tryWriteLockWithConditionState(State expected) {
            State value = state;
            if (expected != value || stateStamp != 0) {
                return false;
            }
            if (compareAndSetStateStamp(0, 1)) {
                //assert stateStamp == 1;
                if (expected != state) {
                    unlockWriteState();
                    return false;
                }
                return true;
            } else {
                return false;
            }
        }

        public boolean multiLockWithConditionState(State read, State write) {
            int stamp;
            State value;
            while (true) {
                stamp = this.stateStamp;
                value = this.state;
                if (value != read && value != write) {
                    return false;
                }
                if (value == read) {
                    if (stamp == 1) {
                        continue;
                    }
                    if (compareAndSetStateStamp(stamp, stamp + 2)) {
                        if (this.state != read) {
                            unlockReadState();
                        } else {
                            return true;
                        }
                    }
                } else {
                    if (stamp != 0) {
                        continue;
                    }
                    if (compareAndSetStateStamp(0, 1)) {
                        if (this.state != write) {
                            unlockWriteState();
                        } else {
                            return true;
                        }
                    }
                }
            }
        }

        public void unlockReadLeft() {
            int stamp;
            while (true) {
                stamp = this.lStamp;
                //assert stamp % 2 == 0 && stamp > 0;
                if (compareAndSetLeftStamp(stamp, stamp - 2))
                    break;
            }
        }

        public void unlockReadRight() {
            int stamp;
            while (true) {
                stamp = this.rStamp;
                //assert stamp % 2 == 0 && stamp > 0;
                if (compareAndSetRightStamp(stamp, stamp - 2))
                    break;
            }
        }

        public void unlockReadState() {
            int stamp;
            while (true) {
                stamp = this.stateStamp;
                //assert stamp % 2 == 0 && stamp > 0;
                if (compareAndSetStateStamp(stamp, stamp - 2))
                    break;
            }
        }

        public void unlockWriteLeft() {
            //assert lStamp == 1;
            lStamp = 0;
        }

        public void unlockWriteRight() {
            //assert rStamp == 1;
            rStamp = 0;
        }

        public void unlockWriteState() {
            //assert stateStamp == 1;
            stateStamp = 0;
        }

        public void multiUnlockState() {
            int stamp = this.stateStamp;
            //assert stamp > 0 && (stamp == 1 || stamp % 2 == 0);
            if (stamp == 1) {
                stateStamp = 0;
            } else {
                while (!compareAndSetStateStamp(stamp, stamp - 2)) {
                    stamp = this.stateStamp;
                }
            }
        }

    }

    private final Node ROOT = new Node(Integer.MAX_VALUE);

    public boolean validateAndTryLock(Node parent, Node child, int key) {
        boolean ret = false;
        if (key < parent.v) {
            ret = parent.tryWriteLockWithConditionLeft(child);
        } else {
            ret = parent.tryWriteLockWithConditionRight(child);
        }
        if (parent.state == State.DELETED) {
            if (ret) {
                if (key < parent.v) {
                    parent.unlockWriteLeft();
                } else {
                    parent.unlockWriteRight();
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
            parent.unlockWriteLeft();
        } else {
            parent.unlockWriteRight();
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
                curr = curr.l;
            } else {
                curr = curr.r;
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
                    switch (curr.state) {
                        case DATA:
                            return false;
                        case DELETED:
                            lockRetry = false;
                            start = ROOT;
                            break;
                        case ROUTING:
                            if (curr.tryWriteLockWithConditionState(State.ROUTING)) {
                                curr.state = State.DATA;
                                curr.unlockWriteState();
                                lockRetry = false;
                                restart = false;
                            }
                    }
                }
            } else {
                final Node node = new Node(v);
                final Node prev = curr;
                prev.readLockState();
                if (validateAndTryLock(prev, null, v)) {
                    node.parent = prev;
                    if (v < prev.v) {
                        prev.l = node;
                    } else {
                        prev.r = node;
                    }
                    undoValidateAndTryLock(prev, node);
                    restart = false;
                } else {
                    if (prev.state == State.DELETED) {
                        start = ROOT;
                    } else {
                        start = prev;
                    }
                }
                prev.unlockReadState();
            }
        }
        return true;
    }

    public boolean removeInt(int v) {
        boolean restart = true;
        final Node curr = traverse(v, ROOT);
        while (restart) {
            if (curr.v != v || curr.state != State.DATA) {
                return false;
            }
            if (!curr.tryWriteLockWithConditionState(State.DATA)) {
                continue;
            }
            switch (curr.numberOfChildren()) {
                case 2: {
                    curr.state = State.ROUTING;
                    restart = false;
                    break;
                }
                case 1: {
                    final Node child;
                    boolean left = false;
                    if (curr.l != null) {
                        left = true;
                        curr.writeLockLeft();
                        child = curr.l;
                    } else {
                        curr.writeLockRight();
                        child = curr.r;
                    }
                    final Node prev = curr.parent;
                    if (!validateAndTryLock(prev, curr)) {
                        if (left) {
                            curr.unlockWriteLeft();
                        } else {
                            curr.unlockWriteRight();
                        }
                        break;
                    }
                    curr.state = State.DELETED;
                    child.parent = prev;
                    if (curr.v < prev.v) {
                        prev.l = child;
                    } else {
                        prev.r = child;
                    }
                    undoValidateAndTryLock(prev, curr);
                    if (left) {
                        curr.unlockWriteLeft();
                    } else {
                        curr.unlockWriteRight();
                    }
                    restart = false;
                    break;
                }
                case 0: {
                    final Node prev = curr.parent;
//                    if (!prev.multiLockWithConditionState(State.DATA, State.ROUTING)) {
//                        break;
//                    }
                    prev.readLockState();
                    if (!validateAndTryLock(prev, curr)) {
//                        prev.unlockWriteState();
//                        prev.multiUnlockState();
                        prev.unlockReadState();
                        break;
                    }
                    if (prev.state == State.DATA) {
                        curr.state = State.DELETED;
                        if (curr.v < prev.v) {
                            prev.l = null;
                        } else {
                            prev.r = null;
                        }
                    } else {
                        //assert prev.state != State.DELETED;
                        final Node child;
                        boolean leftChild = false;
                        if (curr.v < prev.v) {
                            prev.readLockRight();
                            child = prev.r;
                            //assert child.state != State.DELETED;
                        } else {
                            prev.readLockLeft();
                            child = prev.l;
                            //assert child.state != State.DELETED;
                            leftChild = true;
                        }
                        final Node gprev = prev.parent;
                        if (!validateAndTryLock(gprev, prev)) {
                            if (leftChild) {
                                prev.unlockReadLeft();
                            } else {
                                prev.unlockReadRight();
                            }
                            undoValidateAndTryLock(prev, curr);
//                            prev.unlockWriteState();
//                            prev.multiUnlockState();
                            prev.unlockReadState();
                            break;
                        }
                        prev.state = State.DELETED;
                        curr.state = State.DELETED;
                        child.parent = gprev;
                        if (prev.v < gprev.v) {
                            gprev.l = child;
                        } else {
                            gprev.r = child;
                        }
                        //assert child.state != State.DELETED;
                        //assert gprev.state != State.DELETED;
                        undoValidateAndTryLock(gprev, prev);
                        if (leftChild) {
                            prev.unlockReadLeft();
                        } else {
                            prev.unlockReadRight();
                        }
                    }
                    restart = false;
                    undoValidateAndTryLock(prev, curr);
//                    prev.unlockWriteState();
//                    prev.multiUnlockState();
                    prev.unlockReadState();
                }
            }
            curr.unlockWriteState();
        }
        return true;
    }

    public boolean containsInt(int v) {
        Node curr = ROOT;
        while (curr != null) {
            if (curr.v == v) {
                return curr.state == State.DATA;
            }
            if (v < curr.v) {
                curr = curr.l;
            } else {
                curr = curr.r;
            }
        }
        return false;
    }

    public int size(Node v) {
        if (v == null) {
            return 0;
        }
        assert v.state != State.DELETED;
        return (v.state == State.DATA ? 1 : 0) + size(v.l) + size(v.r);
    }

    public int size() {
        return size(ROOT) - 1;
    }

    public int hash(Node v, int power) {
        if (v == null) {
            return 0;
        }
        return v.v * power + hash(v.l, power * 239) + hash(v.r, power * 533);
    }

    public int hash() {
        return hash(ROOT.l, 1);
    }

    public int maxDepth(Node v) {
        if (v == null) {
            return 0;
        }
        return 1 + Math.max(maxDepth(v.l), maxDepth(v.r));
    }

    public int sumDepth(Node v, int d) {
        if (v == null) {
            return 0;
        }
        return (v.state == State.DATA ? d : 0) + sumDepth(v.l, d + 1) + sumDepth(v.r, d + 1);
    }

    public int averageDepth() {
        return (sumDepth(ROOT, -1) + 1) / size();
    }

    public int maxDepth() {
        return maxDepth(ROOT) - 1;
    }

    public boolean stopMaintenance() {
        System.out.println("Average depth: " + averageDepth());
        System.out.println("Depth: " + maxDepth());
        System.out.println("Total depth: " + (sumDepth(ROOT, -1) + 1));
        System.out.println("Hash: " + hash());
        return true;
    }

    public int numNodes(Node v) {
        return v == null ? 0 : 1 + numNodes(v.l) + numNodes(v.r);
    }

    public int numNodes() {
        return numNodes(ROOT) - 1;
    }

    public long getStructMods() {
        return 0;
    }

    public void clear() {
        ROOT.l = null;
        ROOT.r = null;
    }
}
