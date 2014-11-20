/* SnapTree - (c) 2009 Stanford University - PPL */

// SnapReferenceArray

package trees.lockbased.stanfordutils;

import java.util.AbstractList;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.atomic.AtomicLongArray;
import java.util.concurrent.atomic.AtomicReferenceArray;

/** Implements a concurrent fixed-size collection with fast clone.  Reads and
 *  writes have volatile semantics.  No provision is provided to grow or shrink
 *  the collection.
 */
public class SnapDoubleArray implements Iterable<Double>, Cloneable {

    // TODO: clean up the internal implementation

    private static final int LOG_BF = 5;
    private static final int BF = 1 << LOG_BF;
    private static final int BF_MASK = BF - 1;

    // Internally this is implemented as an external tree with branching factor
    // BF.  The leaves of the tree are the elements E.  Nodes are considered to
    // be unshared if they have the same generation as the root node.

    private static class Generation {
    }

    private interface Node {
        Node clone();
    }

    private static final class Leaf extends AtomicLongArray implements Node {
        // never modified after initialization of the SnapDoubleArray, but
        // convenient to be non-final
        Generation gen;

        Leaf(final Generation gen, final int length, final double initialValue) {
            super(length);
            this.gen = gen;
            final long bits = Double.doubleToRawLongBits(initialValue);
            if (bits != 0L) {
                for (int i = 0; i < length; ++i) {
                    lazySet(i, bits);
                }
            }
        }

        Leaf(final Generation gen, final Leaf src) {
            super(src.length());
            this.gen = gen;
            for (int i = 0; i < src.length(); ++i) {
                lazySet(i, src.get(i));
            }
        }

        public Leaf clone() {
            return new Leaf(new Generation(), this);
        }

        double getDouble(final int index) {
            return Double.longBitsToDouble(get(index));
        }

        void setDouble(final int index, final double newValue) {
            set(index, Double.doubleToRawLongBits(newValue));
        }

        double getAndSetDouble(final int index, final double newValue) {
            return Double.longBitsToDouble(getAndSet(index, Double.doubleToRawLongBits(newValue)));
        }

        boolean compareAndSetDouble(final int index, final double expected, final double newValue) {
            return compareAndSet(index, Double.doubleToRawLongBits(expected), Double.doubleToRawLongBits(newValue));
        }
    }

    private static final class Branch extends AtomicReferenceArray<Node> implements Node {
        // never modified after initialization of the SnapReferenceArray, but
        // convenient to be non-final
        Generation gen;

        Branch(final Generation gen, final int length, final Node initialValue) {
            super(length);
            this.gen = gen;
            if (initialValue != null) {
                for (int i = 0; i < length; ++i) {
                    lazySet(i, initialValue);
                }
            }
        }

        Branch(final Generation gen, final Branch src) {
            super(src.length());
            this.gen = gen;
            for (int i = 0; i < src.length(); ++i) {
                lazySet(i, src.get(i));
            }
        }

        public Branch clone() {
            return new Branch(new Generation(), this);
        }
    }

    private static class COWMgr extends CopyOnWriteManager<Node> {
        private COWMgr(final Node initialValue) {
            super(initialValue, 0);
        }

        protected Node freezeAndClone(final Node value) {
            return value.clone();
        }

        protected Node cloneFrozen(final Node frozenValue) {
            return frozenValue.clone();
        }
    }

    /** 0 if _length == 0, otherwise the smallest positive int such that
     *  (1L << (LOG_BF * _height)) >= _length.
     */
    private final int _height;

    private final int _length;

    private CopyOnWriteManager<Node> _rootRef;

    public SnapDoubleArray(final int length) {
        this(length, 0.0);
    }

    public SnapDoubleArray(final int length, final double element) {
        int height = 0;
        Node partial = null;

        if (length > 0) {
            // We will insert the gen into all of the partials (since they
            // are used exactly once).  We reuse the fulls, so we will give
            // them a null gen that will cause them to be copied before any
            // actual writes.
            final Generation gen = new Generation();

            Object full = Double.valueOf(element);

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
                    if (height == 1) {
                        newP = new Leaf(gen, partialBF, (Double) full);
                    }
                    else {
                        newP = new Branch(gen, partialBF, (Node) full);
                        if (partial != null) {
                            ((Branch) newP).set(partialBF - 1, partial);
                        }
                    }
                    assert(partial != null || partialBF < BF);
                }

                Node newF = null;
                if (levelSize > BF || newP == null) {
                    if (height == 1) {
                        newF = new Leaf(null, BF, (Double) full);
                    }
                    else {
                        newF = new Branch(null, BF, (Node) full);
                    }
                }

                if (levelSize <= BF) {
                    // we're done
                    if (newP == null) {
                        // top level is a full, which isn't duplicated
                        if (height == 1) {
                            ((Leaf) newF).gen = gen;
                        }
                        else {
                            ((Branch) newF).gen = gen;
                        }
                        partial = newF;
                    }
                    else {
                        // Top level is a partial.  If it uses exactly one
                        // full child, then we can mark that as unshared.
                        final Branch b = (Branch) newP;
                        if (b.length() == 2 && b.get(0) != b.get(1)) {
                            if (height == 2) {
                                ((Leaf) b.get(0)).gen = gen;
                            }
                            else {
                                ((Branch) b.get(0)).gen = gen;
                            }
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
    public SnapDoubleArray clone() {
        final SnapDoubleArray copy;
        try {
            copy = (SnapDoubleArray) super.clone();
        }
        catch (final CloneNotSupportedException xx) {
            throw new Error("unexpected", xx);
        }
        // height and length are done properly by the magic Cloneable.clone()
        copy._rootRef = new COWMgr(_rootRef.frozen().clone());
        return copy;
    }

    public int length() {
        return _length;
    }

    public double get(final int index) {
        checkBounds(index);

        return readableLeaf(_rootRef.read(), index).getDouble(index & BF_MASK);
    }

    private void checkBounds(final int index) {
        if (index < 0 || index >= _length) {
            throw new IndexOutOfBoundsException();
        }
    }

    private Leaf readableLeaf(final Node root, final int index) {
        Node cur = root;
        for (int h = _height - 1; h >= 1; --h) {
            cur = ((Branch) cur).get((index >> (LOG_BF * h)) & BF_MASK);
        }
        return (Leaf) cur;
    }

    public void set(final int index, final double newValue) {
        checkBounds(index);

        final Epoch.Ticket ticket = _rootRef.beginMutation();
        try {
            mutableLeaf(_rootRef.mutable(), index).setDouble(index & BF_MASK, newValue);
        }
        finally {
            ticket.leave(0);
        }
    }

    public double getAndSet(final int index, final double newValue) {
        checkBounds(index);

        final Epoch.Ticket ticket = _rootRef.beginMutation();
        try {
            return mutableLeaf(_rootRef.mutable(), index).getAndSetDouble(index & BF_MASK, newValue);
        }
        finally {
            ticket.leave(0);
        }
    }

    public boolean compareAndSet(final int index, final double expected, final double newValue) {
        checkBounds(index);

        final Epoch.Ticket ticket = _rootRef.beginMutation();
        try {
            return mutableLeaf(_rootRef.mutable(), index).compareAndSetDouble(index & BF_MASK, expected, newValue);
        }
        finally {
            ticket.leave(0);
        }
    }

    private Leaf mutableLeaf(final Node root, final int index) {
        if (_height <= 1) {
            return (Leaf) root;
        }
        else {
            Branch cur = (Branch) root;
            final Generation gen = cur.gen;
            for (int h = _height - 1; h > 1; --h) {
                cur = mutableChildBranch(gen, cur, h, index);
            }
            return mutableChildLeaf(gen, cur, index);
        }
    }

    private Branch mutableChildBranch(final Generation gen, final Branch cur, final int h, final int index) {
        final int i = (index >> (LOG_BF * h)) & BF_MASK;
        final Branch child = (Branch) cur.get(i);
        if (child.gen == gen) {
            // easy case
            return child;
        }
        else {
            final Branch repl = new Branch(gen, child);

            // reread before CAS
            Node newChild = cur.get(i);
            if (newChild == child) {
                cur.compareAndSet(i, child, repl);
                newChild = cur.get(i);
            }
            return (Branch) newChild;
        }
    }

    private Leaf mutableChildLeaf(final Generation gen, final Branch cur, final int index) {
        final int i = (index >> LOG_BF) & BF_MASK;
        final Leaf child = (Leaf) cur.get(i);
        if (child.gen == gen) {
            // easy case
            return child;
        }
        else {
            final Leaf repl = new Leaf(gen, child);

            // reread before CAS
            Node newChild = cur.get(i);
            if (newChild == child) {
                cur.compareAndSet(i, child, repl);
                newChild = cur.get(i);
            }
            return (Leaf) newChild;
        }
    }

    public Iterator<Double> iterator() {
        final Node root = _rootRef.frozen();
        return new Iterator<Double>() {
            private int _index;
            private Leaf _leaf;

            public boolean hasNext() {
                return _index < _length;
            }

            @SuppressWarnings("unchecked")
            public Double next() {
                if ((_index & BF_MASK) == 0) {
                    _leaf = readableLeaf(root, _index);
                }
                return _leaf.getDouble(_index++ & BF_MASK);
            }

            public void remove() {
                throw new UnsupportedOperationException();
            }
        };
    }

    /** Returns a view of this instance as a fixed-size {@link java.util.List}.  Reads
     *  and writes to the list will be propagated to this instance.
     */
    public List<Double> asList() {
        return new AbstractList<Double>() {
            public Double get(final int index) {
                return SnapDoubleArray.this.get(index);
            }

            @Override
            public Double set(final int index, final Double element) {
                return SnapDoubleArray.this.getAndSet(index, element);
            }

            public int size() {
                return SnapDoubleArray.this.length();
            }

            @Override
            public Iterator<Double> iterator() {
                return SnapDoubleArray.this.iterator();
            }
        };
    }

    @Override
    public String toString() {
        return asList().toString();
    }
}