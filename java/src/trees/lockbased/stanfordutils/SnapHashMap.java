/* SnapTree - (c) 2009 Stanford University - PPL */

// LeafMap

package trees.lockbased.stanfordutils;

import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.Serializable;
import java.util.*;
import java.util.concurrent.ConcurrentMap;
import java.util.concurrent.atomic.AtomicReferenceArray;

/** This SnapHashMap has separate types for the branch and the leaf.  Branches
 *  extend AtomicReferenceArray, and leaves use Java monitors.
 */
public class SnapHashMap<K,V> extends AbstractMap<K,V> implements ConcurrentMap<K,V>, Cloneable, Serializable {
    private static final long serialVersionUID = 9212449426984968891L;

    private static final int LOG_BF = 8;
    private static final int BF = 1 << LOG_BF;
    private static final int BF_MASK = BF - 1;
    private static int maxLoad(final int capacity) { return capacity - (capacity >> 2); /* 0.75 */ }
    private static int minLoad(final int capacity) { return (capacity >> 2); /* 0.25 */ }

    // LEAF_MAX_CAPACITY * 2 / BF must be >= LEAF_MIN_CAPACITY
    private static final int LOG_LEAF_MIN_CAPACITY = 3;
    private static final int LOG_LEAF_MAX_CAPACITY = LOG_LEAF_MIN_CAPACITY + LOG_BF - 1;
    private static final int LEAF_MIN_CAPACITY = 1 << LOG_LEAF_MIN_CAPACITY;
    private static final int LEAF_MAX_CAPACITY = 1 << LOG_LEAF_MAX_CAPACITY;

    private static final int ROOT_SHIFT = 0;

    static class Generation {
    }

    static class HashEntry<K,V> {
        final Generation gen;
        final K key;
        final int hash;
        V value;
        final HashEntry<K,V> next;

        HashEntry(final Generation gen, final K key, final int hash, final V value, final HashEntry<K,V> next) {
            this.gen = gen;
            this.key = key;
            this.hash = hash;
            this.value = value;
            this.next = next;
        }

        HashEntry<K,V> withRemoved(final Generation gen, final HashEntry<K,V> target) {
            if (this == target) {
                return next;
            } else {
                return new HashEntry<K,V>(gen, key, hash, value, next.withRemoved(gen, target));
            }
        }
    }

    /** A small hash table.  The caller is responsible for synchronization in most
     *  cases.
     */
    static final class LeafMap<K,V> {
        /** Set to null when this LeafMap is split into pieces. */
        Generation gen;
        HashEntry<K,V>[] table;

        /** The number of unique hash codes recorded in this LeafMap.  This is also
         *  used to establish synchronization order, by reading in containsKey and
         *  get and writing in any updating function.  We track unique hash codes
         *  instead of entries because LeafMaps are split into multiple LeafMaps
         *  when they grow too large.  If we used a basic count, then if many keys
         *  were present with the same hash code this splitting operation would not
         *  help to restore the splitting condition.
         */
        volatile int uniq;
        final int shift;

        @SuppressWarnings("unchecked")
        LeafMap(final Generation gen, final int shift) {
            this.gen = gen;
            this.table = (HashEntry<K,V>[]) new HashEntry[LEAF_MIN_CAPACITY];
            this.uniq = 0;
            this.shift = shift;
        }

        @SuppressWarnings("unchecked")
        private LeafMap(final Generation gen, final LeafMap src) {
            this.gen = gen;
            this.table = (HashEntry<K,V>[]) src.table.clone();
            this.uniq = src.uniq;
            this.shift = src.shift;
        }

        LeafMap cloneForWrite(final Generation gen) {
            return new LeafMap(gen, this);
        }

        boolean hasSplit() {
            return gen == null;
        }

        boolean containsKey(final Object key, final int hash) {
            if (uniq == 0) { // volatile read
                return false;
            }
            HashEntry<K,V> e = table[(hash >> shift) & (table.length - 1)];
            while (e != null) {
                if (e.hash == hash && key.equals(e.key)) {
                    return true;
                }
                e = e.next;
            }
            return false;
        }

        private synchronized V lockedReadValue(final HashEntry<K,V> e) {
            return e.value;
        }

        /** This is only valid for a quiesced map. */
        boolean containsValue(final Object value) {
            for (HashEntry<K,V> head : table) {
                HashEntry<K,V> e = head;
                while (e != null) {
                    V v = e.value;
                    if (v == null) {
                        v = lockedReadValue(e);
                    }
                    if (value.equals(v)) {
                        return true;
                    }
                    e = e.next;
                }
            }
            return false;
        }

        V get(final Object key, final int hash) {
            if (uniq == 0) { // volatile read
                return null;
            }
            HashEntry<K,V> e = table[(hash >> shift) & (table.length - 1)];
            while (e != null) {
                if (e.hash == hash && key.equals(e.key)) {
                    final V v = e.value;
                    if (v == null) {
                        return lockedReadValue(e);
                    }
                    return v;
                }
                e = e.next;
            }
            return null;
        }

        private void growIfNecessary() {
            assert(!hasSplit());
            final int n = table.length;
            if (n < LEAF_MAX_CAPACITY && uniq > maxLoad(n)) {
                rehash(n << 1);
            }
        }

        private void shrinkIfNecessary() {
            assert(!hasSplit());
            final int n = table.length;
            if (n > LEAF_MIN_CAPACITY && uniq < minLoad(n)) {
                rehash(n >> 1);
            }
        }

        @SuppressWarnings("unchecked")
        private void rehash(final int newSize) {
            final HashEntry<K,V>[] prevTable = table;
            table = (HashEntry<K,V>[]) new HashEntry[newSize];
            for (HashEntry<K,V> head : prevTable) {
                reputAll(head);
            }
        }

        private void reputAll(final HashEntry<K,V> head) {
            if (head != null) {
                reputAll(head.next);
                reput(head);
            }
        }

        private void reput(final HashEntry<K,V> e) {
            final int i = (e.hash >> shift) & (table.length - 1);
            final HashEntry<K,V> next = table[i];
            if (e.next == next) {
                // no new entry needed
                table[i] = e;
            } else {
                table[i] = new HashEntry<K,V>(gen, e.key, e.hash, e.value, next);
            }
        }

        V put(final K key, final int hash, final V value) {
            growIfNecessary();
            final int i = (hash >> shift) & (table.length - 1);
            final HashEntry<K,V> head = table[i];
            HashEntry<K,V> e = head;
            int insDelta = 1;
            while (e != null) {
                if (e.hash == hash) {
                    if (key.equals(e.key)) {
                        // match
                        final V prev = e.value;
                        if (e.gen == gen) {
                            // we have permission to mutate the node
                            e.value = value;
                        } else {
                            // we must replace the node
                            table[i] = new HashEntry<K,V>(gen, key, hash, value, head.withRemoved(gen, e));
                        }
                        uniq = uniq; // volatile store
                        return prev;
                    }
                    // Hash match, but not a key match.  If we eventually insert,
                    // then we won't modify uniq.
                    insDelta = 0;
                }
                e = e.next;
            }
            // no match
            table[i] = new HashEntry<K,V>(gen, key, hash, value, head);
            uniq += insDelta; // volatile store
            return null;
        }

        V remove(final Object key, final int hash) {
            shrinkIfNecessary();
            final int i = (hash >> shift) & (table.length - 1);
            final HashEntry<K,V> head = table[i];
            HashEntry<K,V> e = head;
            int delDelta = -1;
            while (e != null) {
                if (e.hash == hash) {
                    if (key.equals(e.key)) {
                        // match
                        final HashEntry<K,V> target = e;

                        // continue the loop to get the right delDelta
                        if (delDelta != 0) {
                            e = e.next;
                            while (e != null) {
                                if (e.hash == hash) {
                                    delDelta = 0;
                                    break;
                                }
                                e = e.next;
                            }
                        }

                        // match
                        uniq += delDelta; // volatile store
                        table[i] = head.withRemoved(gen, target);
                        return target.value;
                    }
                    // hash match, but not key match
                    delDelta = 0;
                }
                e = e.next;
            }
            // no match
            return null;
        }

        //////// CAS-like

        V putIfAbsent(final K key, final int hash, final V value) {
            growIfNecessary();
            final int i = (hash >> shift) & (table.length - 1);
            final HashEntry<K,V> head = table[i];
            HashEntry<K,V> e = head;
            int insDelta = 1;
            while (e != null) {
                if (e.hash == hash) {
                    if (key.equals(e.key)) {
                        // match => failure
                        return e.value;
                    }
                    // Hash match, but not a key match.  If we eventually insert,
                    // then we won't modify uniq.
                    insDelta = 0;
                }
                e = e.next;
            }
            // no match
            table[i] = new HashEntry<K,V>(gen, key, hash, value, head);
            uniq += insDelta; // volatile store
            return null;
        }

        boolean replace(final K key, final int hash, final V oldValue, final V newValue) {
            final int i = (hash >> shift) & (table.length - 1);
            final HashEntry<K,V> head = table[i];
            HashEntry<K,V> e = head;
            while (e != null) {
                if (e.hash == hash && key.equals(e.key)) {
                    // key match
                    if (oldValue.equals(e.value)) {
                        // CAS success
                        if (e.gen == gen) {
                            // we have permission to mutate the node
                            e.value = newValue;
                        } else {
                            // we must replace the node
                            table[i] = new HashEntry<K,V>(gen, key, hash, newValue, head.withRemoved(gen, e));
                        }
                        uniq = uniq; // volatile store
                        return true;
                    } else {
                        // CAS failure
                        return false;
                    }
                }
                e = e.next;
            }
            // no match
            return false;
        }

        V replace(final K key, final int hash, final V value) {
            final int i = (hash >> shift) & (table.length - 1);
            final HashEntry<K,V> head = table[i];
            HashEntry<K,V> e = head;
            while (e != null) {
                if (e.hash == hash && key.equals(e.key)) {
                    // match
                    final V prev = e.value;
                    if (e.gen == gen) {
                        // we have permission to mutate the node
                        e.value = value;
                    } else {
                        // we must replace the node
                        table[i] = new HashEntry<K,V>(gen, key, hash, value, head.withRemoved(gen, e));
                    }
                    uniq = uniq; // volatile store
                    return prev;
                }
                e = e.next;
            }
            // no match
            return null;
        }

        boolean remove(final Object key, final int hash, final Object value) {
            shrinkIfNecessary();
            final int i = (hash >> shift) & (table.length - 1);
            final HashEntry<K,V> head = table[i];
            HashEntry<K,V> e = head;
            int delDelta = -1;
            while (e != null) {
                if (e.hash == hash) {
                    if (key.equals(e.key)) {
                        // key match
                        if (!value.equals(e.value)) {
                            // CAS failure
                            return false;
                        }

                        final HashEntry<K,V> target = e;

                        // continue the loop to get the right delDelta
                        if (delDelta != 0) {
                            e = e.next;
                            while (e != null) {
                                if (e.hash == hash) {
                                    delDelta = 0;
                                    break;
                                }
                                e = e.next;
                            }
                        }

                        // match
                        uniq += delDelta; // volatile store
                        table[i] = head.withRemoved(gen, target);
                        return true;
                    }
                    // hash match, but not key match
                    delDelta = 0;
                }
                e = e.next;
            }
            // no match
            return false;
        }

        //////// Leaf splitting

        boolean shouldSplit() {
            return uniq > maxLoad(LEAF_MAX_CAPACITY);
        }

        @SuppressWarnings("unchecked")
        LeafMap<K,V>[] split() {
            assert(!hasSplit());

            final LeafMap<K,V>[] pieces = (LeafMap<K,V>[]) new LeafMap[BF];
            for (int i = 0; i < pieces.length; ++i) {
                pieces[i] = new LeafMap<K,V>(gen, shift + LOG_BF);
            }
            for (HashEntry<K,V> head : table) {
                for (HashEntry<K,V> e = head; e != null; e = e.next) {
                    final int i = (e.hash >> shift) & BF_MASK;
                    pieces[i].put(e);
                }
            }

            gen = null; // this marks us as split

            return pieces;
        }

        private void put(final HashEntry<K,V> entry) {
            growIfNecessary();
            final int i = (entry.hash >> shift) & (table.length - 1);
            final HashEntry<K,V> head = table[i];

            // is this hash a duplicate?
            int uDelta = 1;
            for (HashEntry<K,V> e = head; e != null; e = e.next) {
                if (e.hash == entry.hash) {
                    uDelta = 0;
                    break;
                }
            }
            if (uDelta != 0) {
                uniq += uDelta;
            }

            if (entry.next == head) {
                // we can reuse the existing entry
                table[i] = entry;
            } else {
                table[i] = new HashEntry<K,V>(gen, entry.key, entry.hash, entry.value, head);
            }
        }
    }

    static final class BranchMap<K,V> extends AtomicReferenceArray<Object> {
        final Generation gen;
        final int shift;

        BranchMap(final Generation gen, final int shift) {
            super(BF);
            this.gen = gen;
            this.shift = shift;
        }

        BranchMap(final Generation gen, final int shift, final Object[] children) {
            super(children);
            this.gen = gen;
            this.shift = shift;
        }

        private BranchMap(final Generation gen, final BranchMap src) {
            super(BF);
            this.gen = gen;
            this.shift = src.shift;
            for (int i = 0; i < BF; ++i) {
                lazySet(i, src.get(i));
            }
        }

        BranchMap<K,V> cloneForWrite(final Generation gen) {
            return new BranchMap<K,V>(gen, this);
        }

        @SuppressWarnings("unchecked")
        boolean containsKey(final Object key, final int hash) {
            final Object child = get(indexFor(hash));
            if (child == null) {
                return false;
            } else if (child instanceof LeafMap) {
                return ((LeafMap<K,V>) child).containsKey(key, hash);
            } else {
                return ((BranchMap<K,V>) child).containsKey(key, hash);
            }
        }

        private int indexFor(final int hash) {
            return (hash >> shift) & BF_MASK;
        }

        /** This is only valid for a quiesced map. */
        @SuppressWarnings("unchecked")
        boolean containsValueQ(final Object value) {
            for (int i = 0; i < BF; ++i) {
                final Object child = get(i);
                if (child != null) {
                    if (child instanceof LeafMap) {
                        if (((LeafMap<K,V>) child).containsValue(value)) {
                            return true;
                        }
                    } else {
                        if (((BranchMap<K,V>) child).containsValueQ(value)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        @SuppressWarnings("unchecked")
        V get(final Object key, final int hash) {
            final Object child = get(indexFor(hash));
            if (child == null) {
                return null;
            } else if (child instanceof LeafMap) {
                return ((LeafMap<K,V>) child).get(key, hash);
            } else {
                return ((BranchMap<K,V>) child).get(key, hash);
            }
        }

        @SuppressWarnings("unchecked")
        V put(final K key, final int hash, final V value) {
            final int index = indexFor(hash);
            Object child = getOrCreateChild(index);
            while (child instanceof LeafMap) {
                final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                synchronized (leaf) {
                    child = prepareForLeafMutation(index, leaf);
                    if (child == null) {
                        // no replacement was provided
                        return leaf.put(key, hash, value);
                    }
                }
            }
            return unsharedBranch(index, child).put(key, hash, value);
        }

        private Object getOrCreateChild(final int index) {
            final Object result = get(index);
            return result != null ? result : createChild(index);
        }

        private Object createChild(final int index) {
            final LeafMap<K,V> repl = new LeafMap<K,V>(gen, shift + LOG_BF);
            if (compareAndSet(index, null, repl)) {
                // success
                return repl;
            } else {
                // if we failed, someone else succeeded
                return get(index);
            }
        }

        private Object prepareForLeafMutation(final int index, final LeafMap<K,V> leaf) {
            if (leaf.shouldSplit()) {
                if (leaf.hasSplit()) {
                    // leaf was split between our getOrCreateChild and our lock, reread
                    return get(index);
                } else {
                    // no need to CAS, because everyone is using the lock
                    final Object repl = new BranchMap<K,V>(gen, shift + LOG_BF, leaf.split());
                    lazySet(index, repl);
                    return repl;
                }
            } else if (leaf.gen != gen) {
                // copy-on-write
                final Object repl = leaf.cloneForWrite(gen);
                lazySet(index, repl);
                return repl;
            } else {
                // OKAY
                return null;
            }
        }

        @SuppressWarnings("unchecked")
        private BranchMap<K,V> unsharedBranch(final int index, final Object child) {
            final BranchMap<K,V> branch = (BranchMap<K,V>) child;
            if (branch.gen == gen) {
                return branch;
            } else {
                final BranchMap<K,V> fresh = branch.cloneForWrite(gen);
                if (compareAndSet(index, child, fresh)) {
                    return fresh;
                } else {
                    // if we failed someone else succeeded
                    return (BranchMap<K,V>) get(index);
                }
            }
        }

        @SuppressWarnings("unchecked")
        V remove(final Object key, final int hash) {
            final int index = indexFor(hash);
            Object child = getOrCreateChild(index);
            while (child instanceof LeafMap) {
                final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                synchronized (leaf) {
                    child = prepareForLeafMutation(index, leaf);
                    if (child == null) {
                        // no replacement was provided
                        return leaf.remove(key, hash);
                    }
                }
            }
            return unsharedBranch(index, child).remove(key, hash);
        }

        //////// CAS-like

        @SuppressWarnings("unchecked")
        V putIfAbsent(final K key, final int hash, final V value) {
            final int index = indexFor(hash);
            Object child = getOrCreateChild(index);
            while (child instanceof LeafMap) {
                final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                synchronized (leaf) {
                    child = prepareForLeafMutation(index, leaf);
                    if (child == null) {
                        // no replacement was provided
                        return leaf.putIfAbsent(key, hash, value);
                    }
                }
            }
            return unsharedBranch(index, child).putIfAbsent(key, hash, value);
        }

        @SuppressWarnings("unchecked")
        boolean replace(final K key, final int hash, final V oldValue, final V newValue) {
            final int index = indexFor(hash);
            Object child = getOrCreateChild(index);
            while (child instanceof LeafMap) {
                final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                synchronized (leaf) {
                    child = prepareForLeafMutation(index, leaf);
                    if (child == null) {
                        // no replacement was provided
                        return leaf.replace(key, hash, oldValue, newValue);
                    }
                }
            }
            return unsharedBranch(index, child).replace(key, hash, oldValue, newValue);
        }

        @SuppressWarnings("unchecked")
        V replace(final K key, final int hash, final V value) {
            final int index = indexFor(hash);
            Object child = getOrCreateChild(index);
            while (child instanceof LeafMap) {
                final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                synchronized (leaf) {
                    child = prepareForLeafMutation(index, leaf);
                    if (child == null) {
                        // no replacement was provided
                        return leaf.replace(key, hash, value);
                    }
                }
            }
            return unsharedBranch(index, child).replace(key, hash, value);
        }

        @SuppressWarnings("unchecked")
        boolean remove(final Object key, final int hash, final Object value) {
            final int index = indexFor(hash);
            Object child = getOrCreateChild(index);
            while (child instanceof LeafMap) {
                final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                synchronized (leaf) {
                    child = prepareForLeafMutation(index, leaf);
                    if (child == null) {
                        // no replacement was provided
                        return leaf.remove(key, hash, value);
                    }
                }
            }
            return unsharedBranch(index, child).remove(key, hash, value);
        }
    }

    static class COWMgr<K,V> extends CopyOnWriteManager<BranchMap<K,V>> {
        COWMgr() {
            super(new BranchMap<K,V>(new Generation(), ROOT_SHIFT), 0);
        }

        COWMgr(final BranchMap<K,V> initialValue, final int initialSize) {
            super(initialValue, initialSize);
        }

        protected BranchMap<K, V> freezeAndClone(final BranchMap<K,V> value) {
            return value.cloneForWrite(new Generation());
        }

        protected BranchMap<K, V> cloneFrozen(final BranchMap<K,V> frozenValue) {
            return frozenValue.cloneForWrite(new Generation());
        }
    }

    private transient volatile COWMgr<K,V> rootHolder;

    private static int hash(int h) {
        // taken from ConcurrentHashMap
        h += (h <<  15) ^ 0xffffcd7d;
        h ^= (h >>> 10);
        h += (h <<   3);
        h ^= (h >>>  6);
        h += (h <<   2) + (h << 14);
        return h ^ (h >>> 16);
    }

    //////// construction and cloning

    public SnapHashMap() {
        this.rootHolder = new COWMgr<K,V>();
    }

    public SnapHashMap(final Map<? extends K, ? extends V> source) {
        this.rootHolder = new COWMgr<K,V>();
        putAll(source);
    }

    @SuppressWarnings("unchecked")
    public SnapHashMap(final SortedMap<K,? extends V> source) {
        if (source instanceof SnapHashMap) {
            final SnapHashMap<K,V> s = (SnapHashMap<K,V>) source;
            this.rootHolder = (COWMgr<K,V>) s.rootHolder.clone();
        }
        else {
            this.rootHolder = new COWMgr<K,V>();
            putAll(source);
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public SnapHashMap<K,V> clone() {
        final SnapHashMap<K,V> copy;
        try {
            copy = (SnapHashMap<K,V>) super.clone();
        } catch (final CloneNotSupportedException xx) {
            throw new InternalError();
        }
        copy.rootHolder = (COWMgr<K,V>) rootHolder.clone();
        return copy;
    }

    //////// public interface

    public void clear() {
        rootHolder = new COWMgr<K,V>();
    }

    public boolean isEmpty() {
        return rootHolder.isEmpty();
    }

    public int size() {
        return rootHolder.size();
    }

    public boolean containsKey(final Object key) {
        return rootHolder.read().containsKey(key, hash(key.hashCode()));
    }

    public boolean containsValue(final Object value) {
        if (value == null) {
            throw new NullPointerException();
        }
        return rootHolder.frozen().containsValueQ(value);
    }

    @SuppressWarnings("unchecked")
    public V get(final Object key) {
        //return rootHolder.read().get(key, hash(key.hashCode()));
        final int hash = hash(key.hashCode());
        BranchMap<K,V> node = rootHolder.read();
        while (true) {
            final Object child = node.get(node.indexFor(hash));
            if (child instanceof LeafMap) {
                return ((LeafMap<K,V>) child).get(key, hash);
            } else if (child == null) {
                return null;
            }
            // else recurse
            node = (BranchMap<K,V>) child;
        }
    }

    public V put(final K key, final V value) {
        if (value == null) {
            throw new NullPointerException();
        }
        final int h = hash(key.hashCode());
        final Epoch.Ticket ticket = rootHolder.beginMutation();
        int sizeDelta = 0;
        try {
            final V prev = rootHolder.mutable().put(key, h, value);
            if (prev == null) {
                sizeDelta = 1;
            }
            return prev;
        } finally {
            ticket.leave(sizeDelta);
        }
    }

    public V remove(final Object key) {
        final int h = hash(key.hashCode());
        final Epoch.Ticket ticket = rootHolder.beginMutation();
        int sizeDelta = 0;
        try {
            final V prev = rootHolder.mutable().remove(key, h);
            if (prev != null) {
                sizeDelta = -1;
            }
            return prev;
        } finally {
            ticket.leave(sizeDelta);
        }
    }

    //////// CAS-like

    public V putIfAbsent(final K key, final V value) {
        if (value == null) {
            throw new NullPointerException();
        }
        final int h = hash(key.hashCode());
        final Epoch.Ticket ticket = rootHolder.beginMutation();
        int sizeDelta = 0;
        try {
            final V prev = rootHolder.mutable().putIfAbsent(key, h, value);
            if (prev == null) {
                sizeDelta = 1;
            }
            return prev;
        } finally {
            ticket.leave(sizeDelta);
        }
    }

    public boolean replace(final K key, final V oldValue, final V newValue) {
        if (oldValue == null || newValue == null) {
            throw new NullPointerException();
        }
        final int h = hash(key.hashCode());
        final Epoch.Ticket ticket = rootHolder.beginMutation();
        try {
            return rootHolder.mutable().replace(key, h, oldValue, newValue);
        } finally {
            ticket.leave(0);
        }
    }

    public V replace(final K key, final V value) {
        if (value == null) {
            throw new NullPointerException();
        }
        final int h = hash(key.hashCode());
        final Epoch.Ticket ticket = rootHolder.beginMutation();
        try {
            return rootHolder.mutable().replace(key, h, value);
        } finally {
            ticket.leave(0);
        }
    }

    public boolean remove(final Object key, final Object value) {
        final int h = hash(key.hashCode());
        if (value == null) {
            return false;
        }
        final Epoch.Ticket ticket = rootHolder.beginMutation();
        int sizeDelta = 0;
        try {
            final boolean result = rootHolder.mutable().remove(key, h, value);
            if (result) {
                sizeDelta = -1;
            }
            return result;
        } finally {
            ticket.leave(sizeDelta);
        }
    }

    public Set<K> keySet() {
        return new KeySet();
    }

    public Collection<V> values() {
        return new Values();
    }

    public Set<Entry<K,V>> entrySet() {
        return new EntrySet();
    }

    //////// Legacy methods

    public boolean contains(final Object value) {
        return containsValue(value);
    }

    public Enumeration<K> keys() {
        return new KeyIterator(rootHolder.frozen());
    }

    public Enumeration<V> elements() {
        return new ValueIterator(rootHolder.frozen());
    }

    //////// Map support classes

    class KeySet extends AbstractSet<K> {
        public Iterator<K> iterator() {
            return new KeyIterator(rootHolder.frozen());
        }
        public boolean isEmpty() {
            return SnapHashMap.this.isEmpty();
        }
        public int size() {
            return SnapHashMap.this.size();
        }
        public boolean contains(Object o) {
            return SnapHashMap.this.containsKey(o);
        }
        public boolean remove(Object o) {
            return SnapHashMap.this.remove(o) != null;
        }
        public void clear() {
            SnapHashMap.this.clear();
        }
    }

    final class Values extends AbstractCollection<V> {
        public Iterator<V> iterator() {
            return new ValueIterator(rootHolder.frozen());
        }
        public boolean isEmpty() {
            return SnapHashMap.this.isEmpty();
        }
        public int size() {
            return SnapHashMap.this.size();
        }
        public boolean contains(Object o) {
            return SnapHashMap.this.containsValue(o);
        }
        public void clear() {
            SnapHashMap.this.clear();
        }
    }

    final class EntrySet extends AbstractSet<Entry<K,V>> {
        public Iterator<Entry<K,V>> iterator() {
            return new EntryIterator(rootHolder.frozen());
        }
        public boolean contains(Object o) {
            if (!(o instanceof Entry))
                return false;
            Entry<?,?> e = (Entry<?,?>)o;
            V v = SnapHashMap.this.get(e.getKey());
            return v != null && v.equals(e.getValue());
        }
        public boolean remove(Object o) {
            if (!(o instanceof Entry))
                return false;
            Entry<?,?> e = (Entry<?,?>)o;
            return SnapHashMap.this.remove(e.getKey(), e.getValue());
        }
        public boolean isEmpty() {
            return SnapHashMap.this.isEmpty();
        }
        public int size() {
            return SnapHashMap.this.size();
        }
        public void clear() {
            SnapHashMap.this.clear();
        }
    }

    abstract class AbstractIter {

        private final BranchMap<K,V> root;
        private LeafMap<K,V> currentLeaf;
        private HashEntry<K,V> currentEntry;
        private HashEntry<K,V> prevEntry;

        AbstractIter(final BranchMap<K,V> frozenRoot) {
            this.root = frozenRoot;
            findFirst(frozenRoot, 0);
        }

        @SuppressWarnings("unchecked")
        private boolean findFirst(final BranchMap<K,V> branch, final int minIndex) {
            for (int i = minIndex; i < BF; ++i) {
                final Object child = branch.get(i);
                if (child != null) {
                    if (child instanceof LeafMap) {
                        final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                        if (leaf.uniq > 0) {
                            // success!
                            for (HashEntry<K,V> e : leaf.table) {
                                if (e != null) {
                                    currentLeaf = leaf;
                                    currentEntry = e;
                                    return true;
                                }
                            }
                            throw new Error("logic error");
                        }
                    } else {
                        if (findFirst((BranchMap<K,V>) child, 0)) {
                            return true;
                        }
                    }
                }
            }
            return false;
        }

        private void advance() {
            // advance within a bucket chain
            if (currentEntry.next != null) {
                // easy
                currentEntry = currentEntry.next;
                return;
            }

            // advance to the next non-empty chain
            int i = ((currentEntry.hash >> currentLeaf.shift) & (currentLeaf.table.length - 1)) + 1;
            while (i < currentLeaf.table.length) {
                if (currentLeaf.table[i] != null) {
                    currentEntry = currentLeaf.table[i];
                    return;
                }
                ++i;
            }

            // now we are moving between LeafMap-s
            if (!findSuccessor(root)) {
                currentEntry = null;
            }
        }

        @SuppressWarnings("unchecked")
        private boolean findSuccessor(final BranchMap<K,V> branch) {
            final int h = currentEntry.hash;
            final int i = (h >> branch.shift) & BF_MASK;
            final Object child = branch.get(i);
            return (child instanceof BranchMap && findSuccessor((BranchMap<K,V>) child))
                    || findFirst(branch, i + 1);
        }

        public boolean hasNext() {
            return currentEntry != null;
        }

        public boolean hasMoreElements() {
            return hasNext();
        }

        HashEntry<K,V> nextEntry() {
            if (currentEntry == null) {
                throw new NoSuchElementException();
            }
            prevEntry = currentEntry;
            advance();
            return prevEntry;
        }

        public void remove() {
            if (prevEntry == null) {
                throw new IllegalStateException();
            }
            SnapHashMap.this.remove(prevEntry.key);
            prevEntry = null;
        }
    }

    final class KeyIterator extends AbstractIter implements Iterator<K>, Enumeration<K>  {
        KeyIterator(final BranchMap<K, V> frozenRoot) {
            super(frozenRoot);
        }

        public K next() {
            return nextEntry().key;
        }

        public K nextElement() {
            return next();
        }
    }

    final class ValueIterator extends AbstractIter implements Iterator<V>, Enumeration<V> {
        ValueIterator(final BranchMap<K, V> frozenRoot) {
            super(frozenRoot);
        }

        public V next() {
            return nextEntry().value;
        }

        public V nextElement() {
            return next();
        }
    }

    final class WriteThroughEntry extends SimpleEntry<K,V> {
        WriteThroughEntry(final K k, final V v) {
            super(k, v);
        }

	public V setValue(final V value) {
            if (value == null) {
                throw new NullPointerException();
            }
            final V prev = super.setValue(value);
            SnapHashMap.this.put(getKey(), value);
            return prev;
        }
    }

    final class EntryIterator extends AbstractIter implements Iterator<Entry<K,V>> {
        EntryIterator(final BranchMap<K, V> frozenRoot) {
            super(frozenRoot);
        }

        public Entry<K,V> next() {
            final HashEntry<K,V> e = nextEntry();
            return new WriteThroughEntry(e.key, e.value);
        }
    }

    //////// Serialization

    /** Saves the state of the <code>SnapTreeMap</code> to a stream. */
    private void writeObject(final ObjectOutputStream xo) throws IOException {
        // this handles the comparator, and any subclass stuff
        xo.defaultWriteObject();

        // by cloning the COWMgr, we get a frozen tree plus the size
        final COWMgr<K,V> h = (COWMgr<K,V>) rootHolder.clone();

        xo.writeInt(h.size());
        writeEntry(xo, h.frozen());
    }

    @SuppressWarnings("unchecked")
    private void writeEntry(final ObjectOutputStream xo, final BranchMap<K,V> branch) throws IOException {
        for (int i = 0; i < BF; ++i) {
            final Object child = branch.get(i);
            if (child != null) {
                if (child instanceof BranchMap) {
                    writeEntry(xo, (BranchMap<K,V>) child);
                } else {
                    final LeafMap<K,V> leaf = (LeafMap<K,V>) child;
                    for (HashEntry<K,V> head : leaf.table) {
                        HashEntry<K,V> e = head;
                        while (e != null) {
                            xo.writeObject(e.key);
                            xo.writeObject(e.value);
                            e = e.next;
                        }
                    }
                }
            }
        }
    }

    /** Reverses {@link #writeObject(java.io.ObjectOutputStream)}. */
    @SuppressWarnings("unchecked")
    private void readObject(final ObjectInputStream xi) throws IOException, ClassNotFoundException  {
        xi.defaultReadObject();

        final int size = xi.readInt();

        final BranchMap<K,V> root = new BranchMap<K,V>(new Generation(), ROOT_SHIFT);
        for (int i = 0; i < size; ++i) {
            final K k = (K) xi.readObject();
            final V v = (V) xi.readObject();
            root.put(k, hash(k.hashCode()), v);
        }

        rootHolder = new COWMgr<K,V>(root, size);
    }

//    public static void main(final String[] args) {
//        for (int i = 0; i < 10; ++i) {
//            runOne(new SnapHashMap<Integer,String>());
//            runOne(new SnapHashMap<Integer,String>());
//            runOne(new SnapHashMap<Integer,String>());
//            System.out.println();
//            runOne(new java.util.concurrent.ConcurrentHashMap<Integer,String>());
//            runOne(new java.util.concurrent.ConcurrentHashMap<Integer,String>());
//            runOne(new java.util.concurrent.ConcurrentHashMap<Integer,String>());
//            System.out.println();
////            runOne(new SnapTreeMap<Integer,String>());
////            runOne(new java.util.concurrent.ConcurrentSkipListMap<Integer,String>());
//            System.out.println();
//        }
//    }
//
//    private static void runOne(final Map<Integer,String> m) {
//        final long t0 = System.currentTimeMillis();
//        for (int p = 0; p < 10; ++p) {
//            for (int i = 0; i < 100000; ++i) {
//                m.put(Integer.reverse(i), "data");
//            }
//        }
//        final long t1 = System.currentTimeMillis();
//        for (int p = 0; p < 10; ++p) {
//            for (int i = 0; i < 100000; ++i) {
//                m.get(Integer.reverse(i));
//            }
//        }
//        final long t2 = System.currentTimeMillis();
//        for (int p = 0; p < 10; ++p) {
//            for (int i = 0; i < 100000; ++i) {
//                m.get(Integer.reverse(-(i + 1)));
//            }
//        }
//        final long t3 = System.currentTimeMillis();
//        System.out.println(
//                (t1 - t0) + " nanos/put, " +
//                (t2 - t1) + " nanos/get hit, " +
//                (t3 - t2) + " nanos/get miss : " + m.getClass().getSimpleName());
//    }
}