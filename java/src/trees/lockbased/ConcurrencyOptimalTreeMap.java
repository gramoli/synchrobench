package trees.lockbased;

import contention.abstractions.AbstractCompositionalIntSet;
import contention.abstractions.MaintenanceAlg;
import sun.misc.Unsafe;

import java.lang.reflect.Constructor;
import java.util.AbstractMap;

import contention.abstractions.CompositionalMap;

import java.util.Set;
import java.util.Comparator;

/**
 * Created by vaksenov on 16.09.2016.
 */
public class ConcurrencyOptimalTreeMap<K, V> extends AbstractMap<K, V>
        implements CompositionalMap<K, V>, MaintenanceAlg {
    public enum State {
        DATA,
        ROUTING,
        DELETED
    }

    private static final Unsafe unsafe;
    private static final long stateStampOffset, leftStampOffset, rightStampOffset, valueOffset;

    static {
        try {
            Constructor<Unsafe> unsafeConstructor = Unsafe.class.getDeclaredConstructor();
            unsafeConstructor.setAccessible(true);
            unsafe = unsafeConstructor.newInstance();
            stateStampOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("stateStamp"));
            leftStampOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("lStamp"));
            rightStampOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("rStamp"));
            valueOffset = unsafe.objectFieldOffset(Node.class.getDeclaredField("value"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    public static class Node<K, V> {
        final K key;
        volatile V value;
        volatile State state;
        volatile int stateStamp = 0;

        volatile Node<K, V> l;
        volatile int lStamp = 0;

        volatile Node<K, V> r;
        volatile int rStamp = 0;

        volatile Node<K, V> parent;

        public Node(K key, V value) {
            this.key = key;
            this.value = value;
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
            return node.key == key;
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

    private final Node<K, V> ROOT = new Node(null, null);
    private Comparator<? super K> comparator;

    public ConcurrencyOptimalTreeMap() {
    }

    public ConcurrencyOptimalTreeMap(Comparator<? super K> comparator) {
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


    private int compare(final K k1, final K k2) {
        if (comparator == null) {
            return ((Comparable<? super K>) k1).compareTo(k2);
        }
        return comparator.compare(k1, k2);
    }

    public boolean validateAndTryLock(Node<K, V> parent, Node child, boolean left) {
        boolean ret = false;
        if (left) {
            ret = parent.tryWriteLockWithConditionLeft(child);
        } else {
            ret = parent.tryWriteLockWithConditionRight(child);
        }
        if (parent.state == State.DELETED) {
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

    public void undoValidateAndTryLock(Node<K, V> parent, boolean left) {
        if (left) {
            parent.unlockWriteLeft();
        } else {
            parent.unlockWriteRight();
        }
    }

    public void undoValidateAndTryLock(Node<K, V> parent, Node<K, V> child) {
        undoValidateAndTryLock(parent, compare(child.key, parent.key) < 0);
    }

    public Node<K, V> traverse(Object key, Node<K, V> start) {
        Comparable<? super K> k = comparable(key);
        Node<K, V> curr = start;
        Node<K, V> prev = ROOT;
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
                curr = curr.l;
            } else {
                curr = curr.r;
            }
        }
        return prev;
    }

    @Override
    public V putIfAbsent(K key, V value) {
        Node<K, V> start = ROOT.l;
        while (true) {
            final Node<K, V> curr = traverse(key, start);
            final int comparison = curr.key == null ? -1 : compare(key, curr.key);
            if (comparison == 0) {
                boolean lockRetry = true;
                while (lockRetry) {
                    switch (curr.state) {
                        case DATA:
//                            V get = curr.setAndGetNotNull(value); <- for put
                            V get = curr.value;
                            if (get == null) {
                                break;
                            }
                            return get;
                        case DELETED:
                            lockRetry = false;
                            start = ROOT.l;
                            break;
                        case ROUTING:
                            if (curr.tryWriteLockWithConditionState(State.ROUTING)) {
//                                curr.setAndGet(value); <- for put
                                curr.value = value;
                                curr.state = State.DATA;
                                curr.unlockWriteState();
                                return null;
                            }
                    }
                }
            } else {
                final Node<K, V> node = new Node<>(key, value);
                final Node<K, V> prev = curr;
                prev.readLockState();
                final boolean left = comparison < 0;
                if (validateAndTryLock(prev, null, left)) {
                    node.parent = prev;
                    if (left) {
                        prev.l = node;
                    } else {
                        prev.r = node;
                    }
                    undoValidateAndTryLock(prev, left);
                    prev.unlockReadState();
                    return null;
                } else {
                    if (prev.state == State.DELETED) {
                        start = ROOT.l;
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
        final Node<K, V> curr = traverse(key, ROOT.l);
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
                    curr.state = State.ROUTING;
                    restart = false;
                    break;
                }
                case 1: {
                    final Node<K, V> child;
                    boolean leftChild = false;
                    if (curr.l != null) {
                        leftChild = true;
                        curr.writeLockLeft();
                        child = curr.l;
                    } else {
                        curr.writeLockRight();
                        child = curr.r;
                    }
                    final Node<K, V> prev = curr.parent;
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
                    curr.state = State.DELETED;
                    child.parent = prev;
                    if (leftCurr) {
                        prev.l = child;
                    } else {
                        prev.r = child;
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
                    final Node<K, V> prev = curr.parent;
                    if (!prev.multiLockWithConditionState(State.DATA, State.ROUTING)) {
                        break;
                    }
                    final boolean leftCurr = prev.key == null || compare(curr.key, prev.key) < 0;
                    if (!validateAndTryLock(prev, curr, leftCurr)) {
                        prev.multiUnlockState();
                        break;
                    }
                    if (prev.state == State.DATA) {
                        get = curr.setAndGet(null);
                        curr.state = State.DELETED;
                        if (leftCurr) {
                            prev.l = null;
                        } else {
                            prev.r = null;
                        }
                    } else {
                        //assert prev.state != State.DELETED;
                        final Node<K, V> child;
                        boolean leftChild = false;
                        if (leftCurr) {
                            prev.readLockRight();
                            child = prev.r;
                            //assert child.state != State.DELETED;
                        } else {
                            prev.readLockLeft();
                            child = prev.l;
                            //assert child.state != State.DELETED;
                            leftChild = true;
                        }
                        final Node<K, V> gprev = prev.parent;
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
                        prev.state = State.DELETED;
//                        get = curr.setAndGet(null); <- for put
                        get = curr.value;
                        curr.value = null;
                        curr.state = State.DELETED;
                        child.parent = gprev;
                        if (leftPrev) {
                            gprev.l = child;
                        } else {
                            gprev.r = child;
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
        Node<K, V> curr = ROOT.l;
        Comparable<? super K> k = comparable(key);
        int comparison;
        while (curr != null) {
            comparison = k.compareTo(curr.key);
            if (comparison == 0) {
                return curr.value;
            }
            if (comparison < 0) {
                curr = curr.l;
            } else {
                curr = curr.r;
            }
        }
        return null;
    }

    @Override
    public boolean containsKey(Object key) {
        return get(key) != null;
    }

    @Override
    public Set<java.util.Map.Entry<K, V>> entrySet() {
        throw new AssertionError("Entry set is not implemented");
    }

    public int size(Node v) {
        if (v == null) {
            return 0;
        }
        assert v.state != State.DELETED;
        return (v.state == State.DATA ? 1 : 0) + size(v.l) + size(v.r);
    }

    @Override
    public int size() {
        return size(ROOT) - 1;
    }

    public int hash(Node v, int power) {
        if (v == null) {
            return 0;
        }
        return v.key.hashCode() * power + hash(v.l, power * 239) + hash(v.r, power * 533);
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

    @Override
    public void clear() {
        ROOT.l = null;
        ROOT.r = null;
    }
}
