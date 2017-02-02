package trees.lockbased;

import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;
import sun.misc.Unsafe;

import java.lang.reflect.Constructor;
import java.util.AbstractMap;
import java.util.Comparator;
import java.util.Set;

/**
 * Created by vaksenov on 16.09.2016.
 */
public class TConcurrencyOptimalTreeMapv3<K, V> extends AbstractMap<K, V>
        implements CompositionalMap<K, V>, MaintenanceAlg {
    public enum State {
        DATA,
        ROUTING
    }

    private static final Unsafe unsafe;
    private static final long stateStampOffset, leftStampOffset, rightStampOffset, valueOffset;

    static {
        try {
            Constructor<Unsafe> unsafeConstructor = Unsafe.class.getDeclaredConstructor();
            unsafeConstructor.setAccessible(true);
            unsafe = unsafeConstructor.newInstance();
            stateStampOffset = unsafe.objectFieldOffset(TConcurrencyOptimalTreeMapv3.Node.class.getDeclaredField("stateStamp"));
            leftStampOffset = unsafe.objectFieldOffset(TConcurrencyOptimalTreeMapv3.Node.class.getDeclaredField("lStamp"));
            rightStampOffset = unsafe.objectFieldOffset(TConcurrencyOptimalTreeMapv3.Node.class.getDeclaredField("rStamp"));
            valueOffset = unsafe.objectFieldOffset(TConcurrencyOptimalTreeMapv3.Node.class.getDeclaredField("value"));
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    public class Node {
        final K key;
        volatile V value;
        volatile State state;
        volatile int stateStamp = 0;

        volatile Node l;
        volatile int lStamp = 0;

        volatile Node r;
        volatile int rStamp = 0;

        volatile boolean deleted;

        public Node(K key, V value) {
            this.key = key;
            this.value = value;
            state = State.DATA;
            l = null;
            r = null;
        }

        public boolean equals(Object o) {
            if (!(o instanceof TConcurrencyOptimalTreeMapv3.Node)) {
                return false;
            }
            Node node = (Node) o;
            return node.key == key;
        }

        public int numberOfChildren() {
            return (l == null ? 0 : 1) + (r == null ? 0 : 1);
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

        public boolean tryWriteLockWithConditionRefLeft(Node expected) {
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

        public boolean tryWriteLockWithConditionValLeft(Node expected) {
            Node value = l;
            if (lStamp != 0 || (value == null || compare(expected.key, value.key) != 0)) {
                return false;
            }
            if (compareAndSetLeftStamp(0, 1)) {
                if (l == null || compare(expected.key, l.key) != 0) {
                    unlockWriteLeft();
                    return false;
                }
                return true;
            } else {
                return false;
            }
        }

        public boolean tryWriteLockWithConditionRefRight(Node expected) {
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

        public boolean tryWriteLockWithConditionValRight(Node expected) {
            Node value = r;
            if (rStamp != 0 || (value == null || compare(expected.key, value.key) != 0)) {
                return false;
            }
            if (compareAndSetRightStamp(0, 1)) {
                if (r == null || compare(expected.key, r.key) != 0) {
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
            if (expected != value || stateStamp != 0 || deleted) {
                return false;
            }
            if (compareAndSetStateStamp(0, 1)) {
                //assert stateStamp == 1;
                if (expected != state || deleted) {
                    unlockWriteState();
                    return false;
                }
                return true;
            } else {
                return false;
            }
        }

        public boolean tryReadLockWithConditionState(State expected) {
            int stamp;
            State value;
            while (true) {
                stamp = this.stateStamp;
                value = this.state;
                if (expected != value || stamp == 1 || deleted) {
                    return false;
                }
                if (compareAndSetStateStamp(stamp, stamp + 2)) {
                    if (this.state != expected || deleted) {
                        unlockReadState();
                    } else {
                        return true;
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

    private final Node ROOT = new Node(null, null);
    private Comparator<? super K> comparator;

    public TConcurrencyOptimalTreeMapv3() {
    }

    public TConcurrencyOptimalTreeMapv3(Comparator<? super K> comparator) {
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

    public boolean validateRefAndTryLock(Node parent, Node child, boolean left) {
        boolean ret = false;
        if (left) {
            ret = parent.tryWriteLockWithConditionRefLeft(child);
        } else {
            ret = parent.tryWriteLockWithConditionRefRight(child);
        }
        if (parent.deleted) {
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

    public boolean validateValAndTryLock(Node parent, Node child, boolean left) {
        boolean ret = false;
        if (left) {
            ret = parent.tryWriteLockWithConditionValLeft(child);
        } else {
            ret = parent.tryWriteLockWithConditionValRight(child);
        }
        if (parent.deleted) {
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

    public class Window {
        Node curr, prev, gprev;

        public Window() {
            this.prev = ROOT;
            this.curr = ROOT.l;
        }

        public void reset() {
            this.prev = ROOT;
            this.curr = ROOT.l;
        }

        public void set(Node curr, Node prev) {
            this.prev = prev;
            this.curr = curr;
        }

        public void add(Node next) {
            gprev = prev;
            prev = curr;
            curr = next;
        }
    }

    public Window traverse(Object key, Window window) {
        final Comparable<? super K> k = comparable(key);
        int comparison;
        Node curr = window.curr;
        while (curr != null) {
            comparison = k.compareTo(curr.key);
            if (comparison == 0) {
                return window;
            }
            if (comparison < 0) {
                curr = curr.l;
            } else {
                curr = curr.r;
            }
            window.add(curr);
        }
        return window;
    }

    @Override
    public V putIfAbsent(K key, V value) {
        final Window window = new Window();
        while (true) {
            final Node curr = traverse(key, window).curr;
            final int comparison = curr == null ?
                    (window.prev.key == null ? -1 : compare(key, window.prev.key)) :
                    (curr.key == null ? -1 : compare(key, curr.key));
            if (comparison == 0) {
                while (true) {
                    if (curr.deleted) {
                        window.reset();
                        break;
                    }
                    if (curr.state == State.DATA) {
                        V get = curr.value;
                        if (get != null) {
                            return get;
                        }
                    }
                    if (curr.tryWriteLockWithConditionState(State.ROUTING)) { // Already checked on deleted
//                      curr.setAndGet(value); <- for put
                        curr.value = value;
                        curr.state = State.DATA;
                        curr.unlockWriteState();
                        return null;
                    }
                }
            } else {
                final Node prev = window.prev;
                final Node node = new Node(key, value);
                final boolean left = comparison < 0;
                if (validateRefAndTryLock(prev, null, left)) {
                    prev.readLockState();
                    if (!prev.deleted) {
                        if (left) {
                            prev.l = node;
                        } else {
                            prev.r = node;
                        }
                        prev.unlockReadState();
                        undoValidateAndTryLock(prev, left);
                        return null;
                    }
                    prev.unlockReadState();
                    undoValidateAndTryLock(prev, left);
                }
                if (prev.deleted) {
                    window.reset();
                } else {
                    window.set(window.prev, window.gprev);
                }
            }
        }
    }

    public int numberOfChildren(Node l, Node r) {
        return (l == null ? 0 : 1) + (r == null ? 0 : 1);
    }

    public V remove(final Object key) {
        V get = null;
        final Window window = new Window();
        boolean retraverse = true;
        while (true) {
            if (retraverse) {
                window.reset();
                traverse(key, window);
            }
            Node curr = window.curr;
            if (curr == null || curr.state == State.ROUTING || curr.deleted) {
                return null;
            }
            retraverse = false;
            Node left = curr.l;
            Node right = curr.r;
            int nc = numberOfChildren(left, right);
            if (nc == 2) {
                if (!curr.tryWriteLockWithConditionState(State.DATA)) {
                    continue;
                }
                if (curr.numberOfChildren() != 2) {
                    curr.unlockWriteState();
                    continue;
                }
                get = curr.value;
                curr.state = State.ROUTING;
                curr.value = null;
                curr.unlockWriteState();
                return get;
            } else if (nc == 1) {
                final Node child;
                boolean leftChild = false;
                if (left != null) {
                    leftChild = true;
                    child = left;
                } else {
                    child = right;
                }
                final Node prev = window.prev;
                final boolean leftCurr = prev.key == null || compare(curr.key, prev.key) < 0;
                if (!validateRefAndTryLock(curr, child, leftChild)) {
                    continue;
                }
                if (!validateRefAndTryLock(prev, curr, leftCurr)) {
                    undoValidateAndTryLock(curr, leftChild);
                    retraverse = true;
                    continue;
                }
                if (!curr.tryWriteLockWithConditionState(State.DATA)) {
                    undoValidateAndTryLock(prev, leftCurr);
                    undoValidateAndTryLock(curr, leftChild);
                    continue;
                }
                if (curr.numberOfChildren() != 1) {
                    curr.unlockWriteState();
                    undoValidateAndTryLock(prev, leftCurr);
                    undoValidateAndTryLock(curr, leftChild);
                    continue;
                }
//                    get = curr.setAndGet(null); <- for put
                get = curr.value;
                curr.deleted = true;
                if (leftCurr) {
                    prev.l = child;
                } else {
                    prev.r = child;
                }
                curr.unlockWriteState();
                undoValidateAndTryLock(prev, leftCurr);
                undoValidateAndTryLock(curr, leftChild);
                return get;
            } else {
                final Node prev = window.prev;
                final boolean leftCurr = prev.key == null || compare(curr.key, prev.key) < 0;
                if (prev.state == State.DATA) {
                    if (!validateValAndTryLock(prev, curr, leftCurr)) {
                        retraverse = true;
                        continue;
                    }
                    if (leftCurr) {
                        curr = prev.l;
                    } else {
                        curr = prev.r;
                    }
                    if (!curr.tryWriteLockWithConditionState(State.DATA)) {
                        undoValidateAndTryLock(prev, leftCurr);
                        continue;
                    }
                    if (curr.numberOfChildren() != 0) {
                        curr.unlockWriteState();
                        undoValidateAndTryLock(prev, leftCurr);
                        continue;
                    }
                    if (!prev.tryReadLockWithConditionState(State.DATA)) {
                        curr.unlockWriteState();
                        undoValidateAndTryLock(prev, leftCurr);
                        continue;
                    }
//                    get = curr.setAndGet(null);
                    get = curr.value;
                    curr.deleted = true;
                    if (leftCurr) {
                        prev.l = null;
                    } else {
                        prev.r = null;
                    }
                    prev.unlockReadState();
                    curr.unlockWriteState();
                    undoValidateAndTryLock(prev, leftCurr);
                    return get;
                } else {
                    final Node child;
                    boolean leftChild = false;
                    if (leftCurr) {
                        child = prev.r;
                    } else {
                        child = prev.l;
                        leftChild = true;
                    }
                    final Node gprev = window.gprev;
                    final boolean leftPrev = gprev.key == null || compare(prev.key, gprev.key) < 0;
                    if (!validateValAndTryLock(prev, curr, leftCurr)) {
                        retraverse = true;
                        continue;
                    }
                    if (leftCurr) {
                        curr = prev.l;
                    } else {
                        curr = prev.r;
                    }
                    if (!curr.tryWriteLockWithConditionState(State.DATA)) {
                        undoValidateAndTryLock(prev, leftCurr);
                        continue;
                    }
                    if (curr.numberOfChildren() != 0) {
                        curr.unlockWriteState();
                        undoValidateAndTryLock(prev, leftCurr);
                        continue;
                    }
                    if (!validateRefAndTryLock(prev, child, leftChild)) {
                        curr.unlockWriteState();
                        undoValidateAndTryLock(prev, leftCurr);
                        retraverse = true;
                        continue;
                    }
                    if (!validateRefAndTryLock(gprev, prev, leftPrev)) {
                        undoValidateAndTryLock(prev, leftChild);
                        curr.unlockWriteState();
                        undoValidateAndTryLock(prev, leftCurr);
                        retraverse = true;
                        continue;
                    }
                    if (!prev.tryWriteLockWithConditionState(State.ROUTING)) {
                        undoValidateAndTryLock(gprev, leftPrev);
                        undoValidateAndTryLock(prev, leftChild);
                        curr.unlockWriteState();
                        undoValidateAndTryLock(prev, leftCurr);
                        continue;
                    }
                    prev.deleted = true;
//                        get = curr.setAndGet(null); <- for put
                    get = curr.value;
                    curr.deleted = true;
                    if (leftPrev) {
                        gprev.l = child;
                    } else {
                        gprev.r = child;
                    }
                    prev.unlockWriteState();
                    undoValidateAndTryLock(gprev, leftPrev);
                    undoValidateAndTryLock(prev, leftChild);
                    curr.unlockWriteState();
                    undoValidateAndTryLock(prev, leftCurr);
                    return get;
                }
            }
        }
    }

    @Override
    public V get(final Object key) {
        Node curr = ROOT.l;
        final Comparable<? super K> k = comparable(key);
        int comparison;
        while (curr != null) {
            comparison = k.compareTo(curr.key);
            if (comparison == 0) {
                return curr.state == State.DATA && !curr.deleted ? curr.value : null;
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
    public Set<Entry<K, V>> entrySet() {
        throw new AssertionError("Entry set is not implemented");
    }

    public int size(Node v) {
        if (v == null) {
            return 0;
        }
        assert !v.deleted;
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
