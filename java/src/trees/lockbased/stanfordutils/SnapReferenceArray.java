/* SnapTree - (c) 2009 Stanford University - PPL */

// SnapReferenceArray

package trees.lockbased.stanfordutils;

import java.util.AbstractList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.atomic.AtomicReferenceArray;

/** Implements a concurrent fixed-size collection with fast clone.  Reads and
 *  writes have volatile semantics.  No provision is provided to grow or shrink
 *  the collection.  Modelled after {@link AtomicReferenceArray}.
 */
public class SnapReferenceArray<E> implements Iterable<E>, Cloneable {

    private static final int LOG_BF = 5;
    private static final int BF = 1 << LOG_BF;
    private static final int BF_MASK = BF - 1;

    // Internally this is implemented as an external tree with branching factor
    // BF.  The leaves of the tree are the elements E.  Nodes are considered to
    // be unshared if they have the same generation as the root node.

    private static class Generation {
    }

    private static class Node extends AtomicReferenceArray<Object> {
        // never modified after initialization of the SnapReferenceArray, but
        // convenient to be non-final
        Generation gen;

        Node(final Generation gen, final int length, final Object initialValue) {
            super(length);
            this.gen = gen;
            if (initialValue != null) {
                for (int i = 0; i < length; ++i) {
                    lazySet(i, initialValue);
                }
            }
        }

        Node(final Generation gen, final Node src) {
            super(src.length());
            this.gen = gen;
            for (int i = 0; i < src.length(); ++i) {
                lazySet(i, src.get(i));
            }
        }

        Node(final Node src) {
            this(new Generation(), src);
        }
    }

    private static class COWMgr extends CopyOnWriteManager<Node> {
        private COWMgr(final Node initialValue) {
            super(initialValue, 0);
        }

        protected Node freezeAndClone(final Node value) {
            return new Node(value);
        }

        protected Node cloneFrozen(final Node frozenValue) {
            return new Node(frozenValue);
        }
    }

    /** 0 if _length == 0, otherwise the smallest positive int such that
     *  (1L << (LOG_BF * _height)) >= _length.
     */
    private final int _height;

    private final int _length;

    private CopyOnWriteManager<Node> _rootRef;

    public SnapReferenceArray(final int length) {
        this(length, null);
    }

    public SnapReferenceArray(final int length, final E element) {
        int height = 0;
        Node partial = null;

        if (length > 0) {
            // We will insert the gen into all of the partials (since they
            // are used exactly once).  We reuse the fulls, so we will give
            // them a null gen that will cause them to be copied before any
            // actual writes.
            final Generation gen = new Generation();

            Object full = element;

            do {
                ++height;

                // This is the number of nodes required at this level.  They
                // are either all full, or all but one full and one partial.
                int levelSize = ((length - 1) >> (LOG_BF * (height - 1))) + 1;

                // Partial is only present if this level doesn't evenly divide into
                // pieces of length BF, or if a lower level didn't divide evenly.
                Node newP = null;
                if (partial != null || (levelSize & BF_MASK) != 0) {
                    final int partialBF = ((levelSize - 1) & BF_MASK) + 1;
                    newP = new Node(gen, partialBF, full);
                    if (partial != null) {
                        newP.set(partialBF - 1, partial);
                    }
                    assert(partial != null || partialBF < BF);
                }

                Node newF = null;
                if (levelSize > BF || newP == null) {
                    newF = new Node(null, BF, full);
                }

                if (levelSize <= BF) {
                    // we're done
                    if (newP == null) {
                        // top level is a full, which isn't duplicated
                        newF.gen = gen;
                        partial = newF;
                    }
                    else {
                        // Top level is a partial.  If it uses exactly one
                        // full child, then we can mark that as unshared.
                        if (newP.length() == 2 && newP.get(0) != newP.get(1)) {
                            ((Node) newP.get(0)).gen = gen;
                        }
                        partial = newP;
                    }
                    full = null;
                }
                else {
                    partial = newP;
                    full = newF;
                    assert(full != null);
                }
            } while (full != null);
        }

        _height = height;
        _length = length;
        _rootRef = new COWMgr(partial);
    }

    @SuppressWarnings("unchecked")
    public SnapReferenceArray<E> clone() {
        final SnapReferenceArray<E> copy;
        try {
            copy = (SnapReferenceArray<E>) super.clone();
        }
        catch (final CloneNotSupportedException xx) {
            throw new Error("unexpected", xx);
        }
        // height and length are done properly by the magic Cloneable.clone()
        copy._rootRef = new COWMgr(new Node(_rootRef.frozen()));
        return copy;
    }

    public int length() {
        return _length;
    }

    @SuppressWarnings("unchecked")
    public E get(final int index) {
        checkBounds(index);

        return (E) readableLeaf(_rootRef.read(), index).get(index & BF_MASK);
    }

    private void checkBounds(final int index) {
        if (index < 0 || index >= _length) {
            throw new IndexOutOfBoundsException();
        }
    }

    private Node readableLeaf(final Node root, final int index) {
        Node cur = root;
        for (int h = _height - 1; h >= 1; --h) {
            cur = (Node) cur.get((index >> (LOG_BF * h)) & BF_MASK);
        }
        return cur;
    }

    public void set(final int index, final E newValue) {
        checkBounds(index);

        final Epoch.Ticket ticket = _rootRef.beginMutation();
        try {
            mutableLeaf(_rootRef.mutable(), index).set(index & BF_MASK, newValue);
        }
        finally {
            ticket.leave(0);
        }
    }

    @SuppressWarnings("unchecked")
    public E getAndSet(final int index, final E newValue) {
        checkBounds(index);

        final Epoch.Ticket ticket = _rootRef.beginMutation();
        try {
            return (E) mutableLeaf(_rootRef.mutable(), index).getAndSet(index & BF_MASK, newValue);
        }
        finally {
            ticket.leave(0);
        }
    }

    public boolean compareAndSet(final int index, final E expected, final E newValue) {
        checkBounds(index);

        final Epoch.Ticket ticket = _rootRef.beginMutation();
        try {
            return mutableLeaf(_rootRef.mutable(), index).compareAndSet(index & BF_MASK, expected, newValue);
        }
        finally {
            ticket.leave(0);
        }
    }

    private Node mutableLeaf(final Node root, final int index) {
        final Generation gen = root.gen;
        Node cur = root;
        for (int h = _height - 1; h >= 1; --h) {
            final int i = (index >> (LOG_BF * h)) & BF_MASK;
            final Node child = (Node) cur.get(i);
            if (child.gen == gen) {
                // easy case
                cur = child;
                continue;
            }

            final Node repl = new Node(gen, child);

            // reread before CAS
            Node newChild = (Node) cur.get(i);
            if (newChild == child) {
                cur.compareAndSet(i, child, repl);
                newChild = (Node) cur.get(i);
            }
            assert(newChild.gen == gen);
            cur = newChild;
        }
        return cur;
    }

    public Iterator<E> iterator() {
        final Node root = _rootRef.frozen();
        return new Iterator<E>() {
            private int _index;
            private Node _leaf;

            public boolean hasNext() {
                return _index < _length;
            }

            @SuppressWarnings("unchecked")
            public E next() {
                if ((_index & BF_MASK) == 0) {
                    _leaf = readableLeaf(root, _index);
                }
                return (E) _leaf.get(_index++ & BF_MASK);
            }

            public void remove() {
                throw new UnsupportedOperationException();
            }
        };
    }

    /** Returns a view of this instance as a fixed-size {@link List}.  Reads
     *  and writes to the list will be propagated to this instance.
     */ 
    public List<E> asList() {
        return new AbstractList<E>() {
            public E get(final int index) {
                return SnapReferenceArray.this.get(index);
            }

            @Override
            public E set(final int index, final E element) {
                return SnapReferenceArray.this.getAndSet(index, element);
            }

            public int size() {
                return SnapReferenceArray.this.length();
            }

            @Override
            public Iterator<E> iterator() {
                return SnapReferenceArray.this.iterator();
            }
        };
    }

    @Override
    public String toString() {
        return asList().toString();
    }
}
