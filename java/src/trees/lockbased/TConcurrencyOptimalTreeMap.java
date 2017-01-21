package trees.lockbased;

import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;
import sun.misc.Unsafe;

import java.lang.reflect.Constructor;
import java.util.AbstractMap;
import java.util.Comparator;
import java.util.Set;

/**
 * Created by vaksenov on 20.01.2017.
 */
public class TConcurrencyOptimalTreeMap<K, V> extends AbstractMap<K, V>
        implements CompositionalMap<K, V>, MaintenanceAlg {
    public enum State {
        DATA,
        ROUTING,
        DELETED
    }

    private static Unsafe unsafe;
    private static long stateOffset, leftOffset, rightOffset, valueOffset;

    private void init() {
        try {
            Constructor<Unsafe> unsafeConstructor = Unsafe.class.getDeclaredConstructor();
            unsafeConstructor.setAccessible(true);
            unsafe = unsafeConstructor.newInstance();
            stateOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("state"));
            leftOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("l"));
            rightOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("r"));
            valueOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("value"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    public class Pair<T> {
        T value;
        int stamp;

        public Pair() {
        }

        public Pair(T value, int stamp) {
            this.value = value;
            this.stamp = stamp;
        }

        public void set(T value, int stamp) {
            this.value = value;
            this.stamp = stamp;
        }
    }

    public class Node {
        final K key;
        volatile V value;

        volatile Pair<State> state;
        volatile Pair<Node> l;
        volatile Pair<Node> r;

        volatile Node parent;

        public Node(K key, V value) {
            this.key = key;
            this.value = value;
            state = new Pair<>(State.DATA, 0);
            l = new Pair<>(null, 0);
            r = new Pair<>(null, 0);
            parent = null;
        }

        public int numberOfChildren() {
            return (l != null ? 1 : 0) + (r != null ? 1 : 0);
        }

        public boolean equals(Object o) {
//            if (!(o instanceof Node)) {
//                return false;
//            }
            Node node = (Node) o;
            return node.key == key;
        }

        private boolean compareAndSetState(Pair<State> expected, Pair<State> updated) {
            return unsafe.compareAndSwapObject(this, stateOffset, expected, updated);
        }

        private boolean compareAndSetLeft(Pair<Node> expected, Pair<Node> updated) {
            return unsafe.compareAndSwapObject(this, leftOffset, expected, updated);
        }

        private boolean compareAndSetRight(Pair<Node> expected, Pair<Node> updated) {
            return unsafe.compareAndSwapObject(this, rightOffset, expected, updated);
        }

        private boolean compareAndSetValue(V expected, V updated) {
            if (updated == value) {
                return true;
            }
            return unsafe.compareAndSwapObject(this, valueOffset, expected, updated);
        }

        public V setAndGet(V set) {
            V value;
            do {
                value = this.value;
            } while (!compareAndSetValue(value, set));
            return value;
        }

        public V setAndGetNotNull(V set) {
            V value;
            do {
                value = this.value;
            } while (value != null && !compareAndSetValue(value, set));
            return value;
        }

        public void setState(State s) {
            state = new Pair<>(s, state.stamp);
        }

        public void setLeft(Node l) {
            this.l = new Pair<>(l, this.l.stamp);
        }

        public void setRight(Node r) {
            this.r = new Pair<>(r, this.r.stamp);
        }

        public void readLockLeft() {
            Pair<Node> current;
            Pair<Node> replacement = new Pair<>(null, 0);
            while (true) {
                current = l;
                if (current.stamp == 1) {
                    continue;
                }
                replacement.set(current.value, current.stamp + 2);
                if (compareAndSetLeft(current, replacement))
                    break;
            }
        }

        public void readLockRight() {
            Pair<Node> current;
            Pair<Node> replacement = new Pair<>();
            while (true) {
                current = r;
                if (current.stamp == 1) {
                    continue;
                }
                replacement.set(current.value, current.stamp + 2);
                if (compareAndSetRight(current, replacement))
                    break;
            }
        }

        public void readLockState() {
            Pair<State> current;
            Pair<State> replacement = new Pair<>();
            while (true) {
                current = state;
                if (current.stamp == 1) {
                    continue;
                }
                replacement.set(current.value, current.stamp + 2);
                if (compareAndSetState(current, replacement))
                    break;
            }
        }

        public void writeLockLeft() {
            Pair<Node> current;
            Pair<Node> replacement = new Pair<>();
            while (true) {
                current = l;
                if (current.stamp != 0) {
                    continue;
                }
                replacement.set(current.value, 1);
                if (compareAndSetLeft(current, replacement))
                    break;
            }
        }

        public void writeLockRight() {
            Pair<Node> current;
            Pair<Node> replacement = new Pair<>();
            while (true) {
                current = r;
                if (current.stamp != 0) {
                    continue;
                }
                replacement.set(current.value, 1);
                if (compareAndSetRight(current, replacement))
                    break;
            }
        }

        public void writeLockState() {
            Pair<State> current;
            Pair<State> replacement = new Pair<>();
            while (true) {
                current = state;
                if (current.stamp != 0) {
                    continue;
                }
                replacement.set(current.value, 1);
                if (compareAndSetState(current, replacement))
                    break;
            }
        }

        public boolean tryWriteLockWithConditionLeft(Node expected) {
            Pair<Node> current = l;
//            if (expected != value || lStamp != 0) {
            if (current.stamp != 0 && !equality(expected, l.value)) {
                return false;
            }
            Pair<Node> replacement = new Pair<>(expected, 1);
            return compareAndSetLeft(current, replacement);
//            if (compareAndSetLeft(0, 1)) {
//                //assert lStamp == 1;
////                if (expected != l) {
//                if (!equality(expected, l)) {
//                    unlockWriteLeft();
//                    return false;
//                }
//                return true;
//            } else {
//                return false;
//            }
        }

        public boolean tryWriteLockWithConditionRight(Node expected) {
            Pair<Node> current = r;
//            if (expected != value || rStamp != 0) {
            if (current.stamp != 0 || !equality(expected, r.value)) {
                return false;
            }
            Pair<Node> replacement = new Pair<>(expected, 1);
            return compareAndSetRight(current, replacement);
//            if (compareAndSetRightStamp(0, 1)) {
//                //assert rStamp == 1;
////                if (expected != r) {
//                if (!equality(expected, r)) {
//                    unlockWriteRight();
//                    return false;
//                }
//                return true;
//            } else {
//                return false;
//            }
        }

        public boolean tryWriteLockWithConditionState(State expected) {
            Pair<State> current = state;
            if (current.stamp != 0 || expected != current.value) {
                return false;
            }
            Pair<State> replacement = new Pair<>(expected, 1);
            return compareAndSetState(current, replacement);
//            State value = state;
//            if (expected != value || stateStamp != 0) {
//                return false;
//            }
//            if (compareAndSetStateStamp(0, 1)) {
//                //assert stateStamp == 1;
//                if (expected != state) {
//                    unlockWriteState();
//                    return false;
//                }
//                return true;
//            } else {
//                return false;
//            }
        }

        public boolean multiLockWithConditionState(State read, State write) {
            Pair<State> current;
            Pair<State> replacement = new Pair<>();
            while (true) {
                current = state;
                if (current.value != read && current.value != write) {
                    return false;
                }
                if (value == read) {
                    if (current.stamp == 1) {
                        continue;
                    }
                    replacement.set(read, current.stamp + 2);
                    return compareAndSetState(current, replacement);
                } else {
                    if (current.stamp != 0) {
                        continue;
                    }
                    replacement.set(write, 1);
                    return compareAndSetState(current, replacement);
                }
            }
        }

        public void unlockReadLeft() {
            Pair<Node> current = l;
            Pair<Node> replacement = new Pair<>(current.value, current.stamp - 2);
            while (!compareAndSetLeft(current, replacement)) {
                current = l;
                replacement.stamp = current.stamp - 2;
                //assert stamp % 2 == 0 && stamp > 0;
            }
        }

        public void unlockReadRight() {
            Pair<Node> current = r;
            Pair<Node> replacement = new Pair<>(current.value, current.stamp - 2);
            while (!compareAndSetRight(current, replacement)) {
                current = r;
                replacement.stamp = current.stamp - 2;
                //assert stamp % 2 == 0 && stamp > 0;
            }
        }

        public void unlockReadState() {
            Pair<State> current = state;
            Pair<State> replacement = new Pair<>(current.value, current.stamp - 2);
            while (!compareAndSetState(current, replacement)) {
                current = state;
                replacement.stamp = current.stamp - 2;
                //assert stamp % 2 == 0 && stamp > 0;
            }
        }

        public void unlockWriteLeft() {
            //assert lStamp == 1;
            l.stamp = 0;
        }

        public void unlockWriteRight() {
            //assert rStamp == 1;
            r.stamp = 0;
        }

        public void unlockWriteState() {
            //assert stateStamp == 1;
            state.stamp = 0;
        }

        public void multiUnlockState() {
            Pair<State> current = state;
            //assert stamp > 0 && (stamp == 1 || stamp % 2 == 0);
            if (state.stamp == 1) {
                state.stamp = 0;
            } else {
                Pair<State> replacement = new Pair<>(state.value, state.stamp - 2);
                while (!compareAndSetState(current, replacement)) {
                    current = state;
                    replacement.stamp = current.stamp - 2;
                }
            }
        }

    }

    private final Node ROOT = new Node(null, null);
    private Comparator<? super K> comparator;

    public TConcurrencyOptimalTreeMap() {
        init();
    }

    public TConcurrencyOptimalTreeMap(Comparator<? super K> comparator) {
        init();
        this.comparator = comparator;
    }

    private Comparable<? super K> comparable(final Object key) {
        if (key == null) {
            throw new NullPointerException();
        }
        if (comparator == null) {
            return (Comparable<? super K>) key;
        }
        return new Comparable<K>() {
            final Comparator<? super K> _cmp = comparator;

            @SuppressWarnings("unchecked")
            public int compareTo(final K rhs) {
                return _cmp.compare((K) key, rhs);
            }
        };
    }

    private boolean equality(final Node n1, final Node n2) {
        if (comparator == null) {
            return (n1 == null && n2 == null) || (n1 != null && ((Comparable<? super K>) n1.key).compareTo(n2.key) == 0);
        }
        return (n1 == null && n2 == null) || (n1 != null && comparator.compare(n1.key, n2.key) == 0);
    }

    private int compare(final K k1, final K k2) {
        if (comparator == null) {
            return ((Comparable<? super K>) k1).compareTo(k2);
        }
        return comparator.compare(k1, k2);
    }

    public boolean validateAndTryLock(Node parent, Node child, boolean left) {
        boolean ret = false;
        if (left) {
            ret = parent.tryWriteLockWithConditionLeft(child);
        } else {
            ret = parent.tryWriteLockWithConditionRight(child);
        }
        if (parent.state.value == State.DELETED) {
            if (ret) {
                if (left) {
                    parent.unlockWriteLeft();
                } else {
                    parent.unlockWriteRight();
                }
            }
            return false;
        }
        return ret;
    }

    public void undoValidateAndTryLock(Node parent, boolean left) {
        if (left) {
            parent.unlockWriteLeft();
        } else {
            parent.unlockWriteRight();
        }
    }

    public void undoValidateAndTryLock(Node parent, Node child) {
        undoValidateAndTryLock(parent, compare(child.key, parent.key) < 0);
    }

    public Node traverse(Object key, Node start) {
        final Comparable<? super K> k = comparable(key);
        Node curr = start;
        Node prev = ROOT;
//        if (start == ROOT) {
//            prev = ROOT;
//            curr = ROOT.l;
//        }
        int comparison;
        while (curr != null) {
            comparison = k.compareTo(curr.key);
            if (comparison == 0) {
                return curr;
            }
            prev = curr;
            if (comparison < 0) {
                curr = curr.l.value;
            } else {
                curr = curr.r.value;
            }
        }
        return prev;
    }

    @Override
    public V putIfAbsent(K key, V value) {
        Node start = ROOT.l.value;
        while (true) {
            final Node curr = traverse(key, start);
            final int comparison = curr.key == null ? -1 : compare(key, curr.key);
            if (comparison == 0) {
                boolean lockRetry = true;
                while (lockRetry) {
                    switch (curr.state.value) {
                        case DATA:
//                            V get = curr.setAndGetNotNull(value); <- for put
                            V get = curr.value;
                            if (get == null) {
                                break;
                            }
                            return get;
                        case DELETED:
                            lockRetry = false;
                            Node prev = curr.parent;
                            if (prev.state.value != State.DELETED) {
                                start = prev;
                            } else {
                                start = ROOT.l.value;
                            }
                            break;
                        case ROUTING:
                            if (curr.tryWriteLockWithConditionState(State.ROUTING)) {
//                                curr.setAndGet(value); <- for put
                                curr.value = value;
                                curr.setState(State.DATA);
                                curr.unlockWriteState();
                                return null;
                            }
                    }
                }
            } else {
                final Node node = new Node(key, value);
                final Node prev = curr;
                prev.readLockState();
                final boolean left = comparison < 0;
                if (validateAndTryLock(prev, null, left)) {
                    node.parent = prev;
                    if (left) {
                        prev.setLeft(node);
                    } else {
                        prev.setRight(node);
                    }
                    undoValidateAndTryLock(prev, left);
                    prev.unlockReadState();
                    return null;
                } else {
                    if (prev.state.value == State.DELETED) {
                        start = ROOT.l.value;
                    } else {
                        start = prev;
                    }
                }
                prev.unlockReadState();
            }
        }
    }

    public V remove(final Object key) {
        boolean restart = true;
        final Node curr = traverse(key, ROOT.l.value);
        V get = null;
        while (restart) {
            if (compare((K) key, curr.key) != 0 || curr.value == null) {
                return null;
            }
            if (!curr.tryWriteLockWithConditionState(State.DATA)) {
                continue;
            }
            switch (curr.numberOfChildren()) {
                case 2: {
//                    get = curr.setAndGet(null); <- for put
                    get = curr.value;
                    curr.value = null;
                    curr.setState(State.ROUTING);
                    restart = false;
                    break;
                }
                case 1: {
                    final Node child;
                    boolean leftChild = false;
                    if (curr.l != null) {
                        leftChild = true;
                        curr.writeLockLeft();
                        child = curr.l.value;
                    } else {
                        curr.writeLockRight();
                        child = curr.r.value;
                    }
                    final Node prev = curr.parent;
                    final boolean leftCurr = prev.key == null || compare(curr.key, prev.key) < 0;
                    if (!validateAndTryLock(prev, curr, leftCurr)) {
                        if (leftChild) {
                            curr.unlockWriteLeft();
                        } else {
                            curr.unlockWriteRight();
                        }
                        break;
                    }
//                    get = curr.setAndGet(null); <- for put
                    get = curr.value;
                    curr.value = null;
                    curr.setState(State.DELETED);
                    child.parent = prev;
                    if (leftCurr) {
                        prev.setLeft(child);
                    } else {
                        prev.setRight(child);
                    }
                    undoValidateAndTryLock(prev, leftCurr);
                    if (leftChild) {
                        curr.unlockWriteLeft();
                    } else {
                        curr.unlockWriteRight();
                    }
                    restart = false;
                    break;
                }
                case 0: {
                    final Node prev = curr.parent;
                    if (!prev.multiLockWithConditionState(State.DATA, State.ROUTING)) {
                        break;
                    }
                    final boolean leftCurr = prev.key == null || compare(curr.key, prev.key) < 0;
                    if (!validateAndTryLock(prev, curr, leftCurr)) {
                        prev.multiUnlockState();
                        break;
                    }
                    if (prev.state.value == State.DATA) {
                        get = curr.setAndGet(null);
                        curr.setState(State.DELETED);
                        if (leftCurr) {
                            prev.l = null;
                        } else {
                            prev.r = null;
                        }
                    } else {
                        //assert prev.state != State.DELETED;
                        final Node child;
                        boolean leftChild = false;
                        if (leftCurr) {
                            prev.readLockRight();
                            child = prev.r.value;
                            //assert child.state != State.DELETED;
                        } else {
                            prev.readLockLeft();
                            child = prev.l.value;
                            //assert child.state != State.DELETED;
                            leftChild = true;
                        }
                        final Node gprev = prev.parent;
                        final boolean leftPrev = gprev.key == null || compare(prev.key, gprev.key) < 0;
                        if (!validateAndTryLock(gprev, prev, leftPrev)) {
                            if (leftChild) {
                                prev.unlockReadLeft();
                            } else {
                                prev.unlockReadRight();
                            }
                            undoValidateAndTryLock(prev, leftCurr);
//                            prev.unlockWriteState();
                            prev.multiUnlockState();
//                            prev.unlockReadState();
                            break;
                        }
                        prev.setState(State.DELETED);
//                        get = curr.setAndGet(null); <- for put
                        get = curr.value;
                        curr.value = null;
                        curr.setState(State.DELETED);
                        child.parent = gprev;
                        if (leftPrev) {
                            gprev.setLeft(child);
                        } else {
                            gprev.setRight(child);
                        }
                        //assert child.state != State.DELETED;
                        //assert gprev.state != State.DELETED;
                        undoValidateAndTryLock(gprev, leftPrev);
                        if (leftChild) {
                            prev.unlockReadLeft();
                        } else {
                            prev.unlockReadRight();
                        }
                    }
                    restart = false;
                    undoValidateAndTryLock(prev, leftCurr);
//                    prev.unlockWriteState();
                    prev.multiUnlockState();
                }
            }
            curr.unlockWriteState();
        }
        return get;
    }

    @Override
    public V get(final Object key) {
        Node curr = ROOT.l.value;
        final Comparable<? super K> k = comparable(key);
        int comparison;
        while (curr != null) {
            comparison = k.compareTo(curr.key);
            if (comparison == 0) {
                return curr.value;
            }
            if (comparison < 0) {
                curr = curr.l.value;
            } else {
                curr = curr.r.value;
            }
        }
        return null;
    }

    @Override
    public boolean containsKey(Object key) {
        return get(key) != null;
    }

    @Override
    public Set<Entry<K, V>> entrySet() {
        throw new AssertionError("Entry set is not implemented");
    }

    public int size(Node v) {
        if (v == null) {
            return 0;
        }
        assert v.state.value != State.DELETED;
        return (v.state.value == State.DATA ? 1 : 0) + size(v.l.value) + size(v.r.value);
    }

    @Override
    public int size() {
        return size(ROOT) - 1;
    }

    public int hash(Node v, int power) {
        if (v == null) {
            return 0;
        }
        return v.key.hashCode() * power + hash(v.l.value, power * 239) + hash(v.r.value, power * 533);
    }

    public int hash() {
        return hash(ROOT.l.value, 1);
    }

    public int maxDepth(Node v) {
        if (v == null) {
            return 0;
        }
        return 1 + Math.max(maxDepth(v.l.value), maxDepth(v.r.value));
    }

    public int sumDepth(Node v, int d) {
        if (v == null) {
            return 0;
        }
        return (v.state.value == State.DATA ? d : 0) + sumDepth(v.l.value, d + 1) + sumDepth(v.r.value, d + 1);
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
        return v == null ? 0 : 1 + numNodes(v.l.value) + numNodes(v.r.value);
    }

    public int numNodes() {
        return numNodes(ROOT) - 1;
    }

    public long getStructMods() {
        return 0;
    }

    @Override
    public void clear() {
        ROOT.l = new Pair<>();
        ROOT.r = new Pair<>();
    }
}
