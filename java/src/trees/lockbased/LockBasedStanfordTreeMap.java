/*
 * Copyright (c) 2009 Stanford University, unless otherwise specified.
 * All rights reserved.
 *
 * This software was developed by the Pervasive Parallelism Laboratory of
 * Stanford University, California, USA.
 *
 * Permission to use, copy, modify, and distribute this software in source
 * or binary form for any purpose with or without fee is hereby granted,
 * provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* SnapTree - (c) 2009 Stanford University - PPL */

// SnapTreeMap

package trees.lockbased;

import java.util.AbstractMap;
import java.util.AbstractSet;
import java.util.Comparator;
import java.util.Iterator;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Set;

import contention.abstractions.CompositionalMap;
import contention.abstractions.CompositionalMap.Vars;
import trees.lockbased.stanfordutils.SnapTreeMap;

/**
 * A concurrent relaxed balance AVL tree, based on the algorithm of Bronson,
 * Casper, Chafi, and Olukotun, "A Practical Concurrent Binary Search Tree"
 * published in PPoPP'10. To simplify the locking protocols rebalancing work is
 * performed in pieces, and some removed keys are be retained as routing nodes
 * in the tree.
 * 
 * <p>
 * Compared to {@link SnapTreeMap}, this implementation does not provide any
 * structural sharing with copy on write. As a result, it must support iteration
 * on the mutating structure, so nodes track both the number of shrinks (which
 * invalidate queries and traverals) and grows (which invalidate traversals).
 * 
 * @author Nathan Bronson
 */
public class LockBasedStanfordTreeMap<K, V> extends AbstractMap<K, V> implements
		CompositionalMap<K, V> {
	// public class OptTreeMap<K,V> extends AbstractMap<K,V> implements
	// ConcurrentMap<K,V> {

	/**
	 * This is a special value that indicates the presence of a null value, to
	 * differentiate from the absence of a value.
	 */
	static final Object SpecialNull = new Object();

	/**
	 * This is a special value that indicates that an optimistic read failed.
	 */
	static final Object SpecialRetry = new Object();

	/** The number of spins before yielding. */
	static final int SpinCount = Integer.parseInt(System.getProperty("spin",
			"100"));

	/** The number of yields before blocking. */
	static final int YieldCount = Integer.parseInt(System.getProperty("yield",
			"0"));

	static final int OVLBitsBeforeOverflow = Integer.parseInt(System
			.getProperty("shrinkbits", "8"));

	// we encode directions as characters
	static final char Left = 'L';
	static final char Right = 'R';

	// return type for extreme searches
	static final int ReturnKey = 0;
	static final int ReturnEntry = 1;
	static final int ReturnNode = 2;

	/**
	 * An <tt>OVL</tt> is a version number and lock used for optimistic
	 * concurrent control of some program invariant. If {@link #isChanging} then
	 * the protected invariant is changing. If two reads of an OVL are performed
	 * that both see the same non-changing value, the reader may conclude that
	 * no changes to the protected invariant occurred between the two reads. The
	 * special value UnlinkedOVL is not changing, and is guaranteed to not
	 * result from a normal sequence of beginChange and endChange operations.
	 * <p>
	 * For convenience <tt>endChange(ovl) == endChange(beginChange(ovl))</tt>.
	 */
	private static final long UnlinkedOVL = 1L;
	private static final long OVLGrowLockMask = 2L;
	private static final long OVLShrinkLockMask = 4L;
	private static final int OVLGrowCountShift = 3;
	private static final long OVLGrowCountMask = ((1L << OVLBitsBeforeOverflow) - 1) << OVLGrowCountShift;
	private static final long OVLShrinkCountShift = OVLGrowCountShift
			+ OVLBitsBeforeOverflow;

	private static long beginGrow(final long ovl) {
		assert (!isChangingOrUnlinked(ovl));
		return ovl | OVLGrowLockMask;
	}

	private static long endGrow(final long ovl) {
		assert (!isChangingOrUnlinked(ovl));

		// Overflows will just go into the shrink lock count, which is fine.
		return ovl + (1L << OVLGrowCountShift);
	}

	private static long beginShrink(final long ovl) {
		assert (!isChangingOrUnlinked(ovl));
		return ovl | OVLShrinkLockMask;
	}

	private static long endShrink(final long ovl) {
		assert (!isChangingOrUnlinked(ovl));

		// increment overflows directly
		return ovl + (1L << OVLShrinkCountShift);
	}

	private static boolean isChanging(final long ovl) {
		return (ovl & (OVLShrinkLockMask | OVLGrowLockMask)) != 0;
	}

	private static boolean isUnlinked(final long ovl) {
		return ovl == UnlinkedOVL;
	}

	private static boolean isShrinkingOrUnlinked(final long ovl) {
		return (ovl & (OVLShrinkLockMask | UnlinkedOVL)) != 0;
	}

	private static boolean isChangingOrUnlinked(final long ovl) {
		return (ovl & (OVLShrinkLockMask | OVLGrowLockMask | UnlinkedOVL)) != 0;
	}

	private static boolean hasShrunkOrUnlinked(final long orig,
			final long current) {
		return ((orig ^ current) & ~(OVLGrowLockMask | OVLGrowCountMask)) != 0;
	}

	private static boolean hasChangedOrUnlinked(final long orig,
			final long current) {
		return orig != current;
	}

	private static class Node<K, V> {
		final K key;
		volatile int height;

		/**
		 * null means this node is conceptually not present in the map.
		 * SpecialNull means the value is null.
		 */
		volatile Object vOpt;
		volatile Node<K, V> parent;
		volatile long changeOVL;
		volatile Node<K, V> left;
		volatile Node<K, V> right;

		Node(final K key, final int height, final Object vOpt,
				final Node<K, V> parent, final long changeOVL,
				final Node<K, V> left, final Node<K, V> right) {
			this.key = key;
			this.height = height;
			this.vOpt = vOpt;
			this.parent = parent;
			this.changeOVL = changeOVL;
			this.left = left;
			this.right = right;
		}

		Node<K, V> child(char dir) {
			return dir == Left ? left : right;
		}

		Node<K, V> childSibling(char dir) {
			return dir == Left ? right : left;
		}

		void setChild(char dir, Node<K, V> node) {
			if (dir == Left) {
				left = node;
			} else {
				right = node;
			}
		}

		// ////// per-node blocking

		private void waitUntilChangeCompleted(final long ovl) {
			if (!isChanging(ovl)) {
				return;
			}

			for (int tries = 0; tries < SpinCount; ++tries) {
				if (changeOVL != ovl) {
					return;
				}
			}

			for (int tries = 0; tries < YieldCount; ++tries) {
				Thread.yield();
				if (changeOVL != ovl) {
					return;
				}
			}

			// spin and yield failed, use the nuclear option
			synchronized (this) {
				// we can't have gotten the lock unless the shrink was over
			}
			assert (changeOVL != ovl);
		}

		int validatedHeight() {
			final int hL = left == null ? 0 : left.validatedHeight();
			final int hR = right == null ? 0 : right.validatedHeight();
			assert (Math.abs(hL - hR) <= 1);
			final int h = 1 + Math.max(hL, hR);
			assert (h == height);
			return height;
		}
	}

	// ////// node access functions

	private static int height(final Node<?, ?> node) {
		return node == null ? 0 : node.height;
	}

	@SuppressWarnings("unchecked")
	private V decodeNull(final Object vOpt) {
		assert (vOpt != SpecialRetry);
		return vOpt == SpecialNull ? null : (V) vOpt;
	}

	private static Object encodeNull(final Object v) {
		return v == null ? SpecialNull : v;
	}

	// ////////////// state

	private Comparator<? super K> comparator;
	private final Node<K, V> rootHolder = new Node<K, V>(null, 1, null, null,
			0L, null, null);
	private final EntrySet entries = new EntrySet();

	// ////////////// public interface

	public LockBasedStanfordTreeMap() {
	}

	public LockBasedStanfordTreeMap(final Comparator<? super K> comparator) {
		this.comparator = comparator;
	}

	@Override
	public int size() {
		final Iterator<?> iter = entrySet().iterator();
		int n = 0;
		while (iter.hasNext()) {
			iter.next();
			++n;
		}
		return n;
	}

	@Override
	public boolean isEmpty() {
		// removed-but-not-unlinked nodes cannot be leaves, so if the tree is
		// truly empty then the root holder has no right child
		return rootHolder.right == null;
	}

	@Override
	public void clear() {
		synchronized (rootHolder) {
			rootHolder.height = 1;
			rootHolder.right = null;
		}
	}

	public Comparator<? super K> comparator() {
		return comparator;
	}

	// ////// search

	@Override
	public boolean containsKey(final Object key) {
		return getImpl(key) != null;
	}

	@Override
	public V get(final Object key) {
		return decodeNull(getImpl(key));
	}

	@SuppressWarnings("unchecked")
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

	void finishCount1(int nodesTraversed) {
		Vars vars = counts.get();
		vars.getCount++;
		vars.nodesTraversed += nodesTraversed;
	}

	void finishCount2(int nodesTraversed) {
		Vars vars = counts.get();
		vars.nodesTraversed += nodesTraversed;
	}

	/** Returns either a value or SpecialNull, if present, or null, if absent. */
	private Object getImpl(final Object key) {
		final Comparable<? super K> k = comparable(key);

		int nodesTraversed = 0;

		while (true) {
			final Node<K, V> right = rootHolder.right;
			if (TRAVERSAL_COUNT) {
				nodesTraversed++;
			}
			if (right == null) {
				if (TRAVERSAL_COUNT) {
					finishCount1(nodesTraversed);
				}
				return null;
			} else {
				final int rightCmp = k.compareTo(right.key);
				if (rightCmp == 0) {
					// who cares how we got here
					if (TRAVERSAL_COUNT) {
						finishCount1(nodesTraversed);
					}
					return right.vOpt;
				}

				final long ovl = right.changeOVL;
				if (isShrinkingOrUnlinked(ovl)) {
					right.waitUntilChangeCompleted(ovl);
					// RETRY
				} else if (right == rootHolder.right) {
					// the reread of .right is the one protected by our read of
					// ovl
					final Object vo = attemptGet(k, right, (rightCmp < 0 ? Left
							: Right), ovl);
					if (vo != SpecialRetry) {
						if (TRAVERSAL_COUNT) {
							finishCount1(nodesTraversed);
						}
						return vo;
					}
					// else RETRY
				}
			}
		}
	}

	private Object attemptGet(final Comparable<? super K> k,
			final Node<K, V> node, final char dirToC, final long nodeOVL) {
		int nodesTraversed = 0;
		while (true) {
			final Node<K, V> child = node.child(dirToC);
			if (TRAVERSAL_COUNT) {
				nodesTraversed++;
			}

			if (child == null) {
				if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
					if (TRAVERSAL_COUNT) {
						finishCount2(nodesTraversed);
					}
					return SpecialRetry;
				}

				// Note is not present. Read of node.child occurred while
				// parent.child was valid, so we were not affected by any
				// shrinks.
				if (TRAVERSAL_COUNT) {
					finishCount2(nodesTraversed);
				}
				return null;
			} else {
				final int childCmp = k.compareTo(child.key);
				if (childCmp == 0) {
					// how we got here is irrelevant
					if (TRAVERSAL_COUNT) {
						finishCount2(nodesTraversed);
					}
					return child.vOpt;
				}

				// child is non-null
				final long childOVL = child.changeOVL;
				if (isShrinkingOrUnlinked(childOVL)) {
					child.waitUntilChangeCompleted(childOVL);

					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						if (TRAVERSAL_COUNT) {
							finishCount2(nodesTraversed);
						}
						return SpecialRetry;
					}
					// else RETRY
				} else if (child != node.child(dirToC)) {
					// this .child is the one that is protected by childOVL
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						if (TRAVERSAL_COUNT) {
							finishCount2(nodesTraversed);
						}
						return SpecialRetry;
					}
					// else RETRY
				} else {
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						if (TRAVERSAL_COUNT) {
							finishCount2(nodesTraversed);
						}
						return SpecialRetry;
					}

					// At this point we know that the traversal our parent took
					// to get to node is still valid. The recursive
					// implementation will validate the traversal from node to
					// child, so just prior to the nodeOVL validation both
					// traversals were definitely okay. This means that we are
					// no longer vulnerable to node shrinks, and we don't need
					// to validate nodeOVL any more.
					final Object vo = attemptGet(k, child, (childCmp < 0 ? Left
							: Right), childOVL);
					if (vo != SpecialRetry) {
						if (TRAVERSAL_COUNT) {
							finishCount2(nodesTraversed);
						}
						return vo;
					}
					// else RETRY
				}
			}
		}
	}

	@SuppressWarnings("unchecked")
	// TODO: @Override
	public K firstKey() {
		return (K) extreme(ReturnKey, Left);
	}

	@SuppressWarnings("unchecked")
	// TODO: @Override
	public Entry<K, V> firstEntry() {
		return (SimpleImmutableEntry<K, V>) extreme(ReturnEntry, Left);
	}

	@SuppressWarnings("unchecked")
	// TODO: @Override
	public K lastKey() {
		return (K) extreme(ReturnKey, Right);
	}

	@SuppressWarnings("unchecked")
	// TODO: @Override
	public Entry<K, V> lastEntry() {
		return (SimpleImmutableEntry<K, V>) extreme(ReturnEntry, Right);
	}

	/** Returns a key if returnKey is true, a SimpleImmutableEntry otherwise. */
	private Object extreme(final int returnType, final char dir) {
		while (true) {
			final Node<K, V> right = rootHolder.right;
			if (right == null) {
				if (returnType == ReturnNode) {
					return null;
				} else {
					throw new NoSuchElementException();
				}
			} else {
				final long ovl = right.changeOVL;
				if (isShrinkingOrUnlinked(ovl)) {
					right.waitUntilChangeCompleted(ovl);
					// RETRY
				} else if (right == rootHolder.right) {
					// the reread of .right is the one protected by our read of
					// ovl
					final Object vo = attemptExtreme(returnType, dir, right,
							ovl);
					if (vo != SpecialRetry) {
						return vo;
					}
					// else RETRY
				}
			}
		}
	}

	private Object attemptExtreme(final int returnType, final char dir,
			final Node<K, V> node, final long nodeOVL) {
		while (true) {
			final Node<K, V> child = node.child(dir);

			if (child == null) {
				// read of the value must be protected by the OVL, because we
				// must linearize against another thread that inserts a new min
				// key and then changes this key's value
				final Object vo = node.vOpt;

				if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
					return SpecialRetry;
				}

				assert (vo != null);

				switch (returnType) {
				case ReturnKey:
					return node.key;
				case ReturnEntry:
					return new SimpleImmutableEntry<K, V>(node.key,
							decodeNull(vo));
				default:
					return node;
				}
			} else {
				// child is non-null
				final long childOVL = child.changeOVL;
				if (isShrinkingOrUnlinked(childOVL)) {
					child.waitUntilChangeCompleted(childOVL);

					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						return SpecialRetry;
					}
					// else RETRY
				} else if (child != node.child(dir)) {
					// this .child is the one that is protected by childOVL
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						return SpecialRetry;
					}
					// else RETRY
				} else {
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						return SpecialRetry;
					}

					final Object vo = attemptExtreme(returnType, dir, child,
							childOVL);
					if (vo != SpecialRetry) {
						return vo;
					}
					// else RETRY
				}
			}
		}
	}

	// ////////////// update

	private static final int UpdateAlways = 0;
	private static final int UpdateIfAbsent = 1;
	private static final int UpdateIfPresent = 2;
	private static final int UpdateIfEq = 3;

	private static boolean shouldUpdate(final int func, final Object prev,
			final Object expected) {
		switch (func) {
		case UpdateAlways:
			return true;
		case UpdateIfAbsent:
			return prev == null;
		case UpdateIfPresent:
			return prev != null;
		default:
			return prev == expected; // TODO: use .equals
		}
	}

	@Override
	public V put(final K key, final V value) {
		return decodeNull(update(key, UpdateAlways, null, encodeNull(value)));
	}

	@Override
	public V putIfAbsent(final K key, final V value) {
		return decodeNull(update(key, UpdateIfAbsent, null, encodeNull(value)));
	}

	// @Override
	public V replace(final K key, final V value) {
		return decodeNull(update(key, UpdateIfPresent, null, encodeNull(value)));
	}

	// @Override
	public boolean replace(final K key, final V oldValue, final V newValue) {
		return update(key, UpdateIfEq, encodeNull(oldValue),
				encodeNull(newValue)) == encodeNull(oldValue);
	}

	@Override
	public V remove(final Object key) {
		return decodeNull(update(key, UpdateAlways, null, null));
	}

	// @Override
	public boolean remove(final Object key, final Object value) {
		return update(key, UpdateIfEq, encodeNull(value), null) == encodeNull(value);
	}

	@SuppressWarnings("unchecked")
	private Object update(final Object key, final int func,
			final Object expected, final Object newValue) {
		final Comparable<? super K> k = comparable(key);

		while (true) {
			final Node<K, V> right = rootHolder.right;
			if (right == null) {
				// key is not present
				if (!shouldUpdate(func, null, expected) || newValue == null
						|| attemptInsertIntoEmpty((K) key, newValue)) {
					// nothing needs to be done, or we were successful, prev
					// value is Absent
					return null;
				}
				// else RETRY
			} else {
				final long ovl = right.changeOVL;
				if (isShrinkingOrUnlinked(ovl)) {
					right.waitUntilChangeCompleted(ovl);
					// RETRY
				} else if (right == rootHolder.right) {
					// this is the protected .right
					final Object vo = attemptUpdate(key, k, func, expected,
							newValue, rootHolder, right, ovl);
					if (vo != SpecialRetry) {
						return vo;
					}
					// else RETRY
				}
			}
		}
	}

	private boolean attemptInsertIntoEmpty(final K key, final Object vOpt) {
		synchronized (rootHolder) {
			if (rootHolder.right == null) {
				rootHolder.right = new Node<K, V>(key, 1, vOpt, rootHolder, 0L,
						null, null);
				rootHolder.height = 2;
				return true;
			} else {
				return false;
			}
		}
	}

	/**
	 * If successful returns the non-null previous value, SpecialNull for a null
	 * previous value, or null if not previously in the map. The caller should
	 * retry if this method returns SpecialRetry.
	 */
	@SuppressWarnings("unchecked")
	private Object attemptUpdate(final Object key,
			final Comparable<? super K> k, final int func,
			final Object expected, final Object newValue,
			final Node<K, V> parent, final Node<K, V> node, final long nodeOVL) {
		// As the search progresses there is an implicit min and max assumed for
		// the
		// branch of the tree rooted at node. A left rotation of a node x
		// results in
		// the range of keys in the right branch of x being reduced, so if we
		// are at a
		// node and we wish to traverse to one of the branches we must make sure
		// that
		// the node has not undergone a rotation since arriving from the parent.
		//
		// A rotation of node can't screw us up once we have traversed to node's
		// child, so we don't need to build a huge transaction, just a chain of
		// smaller read-only transactions.

		assert (nodeOVL != UnlinkedOVL);

		final int cmp = k.compareTo(node.key);
		if (cmp == 0) {
			return attemptNodeUpdate(func, expected, newValue, parent, node);
		}

		final char dirToC = cmp < 0 ? Left : Right;

		while (true) {
			final Node<K, V> child = node.child(dirToC);

			if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
				return SpecialRetry;
			}

			if (child == null) {
				// key is not present
				if (newValue == null) {
					// Removal is requested. Read of node.child occurred
					// while parent.child was valid, so we were not affected
					// by any shrinks.
					return null;
				} else {
					// Update will be an insert.
					final boolean success;
					final Node<K, V> damaged;
					synchronized (node) {
						// Validate that we haven't been affected by past
						// rotations. We've got the lock on node, so no future
						// rotations can mess with us.
						if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
							return SpecialRetry;
						}

						if (node.child(dirToC) != null) {
							// Lost a race with a concurrent insert. No need
							// to back up to the parent, but we must RETRY in
							// the outer loop of this method.
							success = false;
							damaged = null;
						} else {
							// We're valid. Does the user still want to
							// perform the operation?
							if (!shouldUpdate(func, null, expected)) {
								return null;
							}

							// Create a new leaf
							node.setChild(dirToC, new Node<K, V>((K) key, 1,
									newValue, node, 0L, null, null));
							success = true;

							// attempt to fix node.height while we've still got
							// the lock
							damaged = fixHeight_nl(node);
						}
					}
					if (success) {
						fixHeightAndRebalance(damaged);
						return null;
					}
					// else RETRY
				}
			} else {
				// non-null child
				final long childOVL = child.changeOVL;
				if (isShrinkingOrUnlinked(childOVL)) {
					child.waitUntilChangeCompleted(childOVL);
					// RETRY
				} else if (child != node.child(dirToC)) {
					// this second read is important, because it is protected
					// by childOVL
					// RETRY
				} else {
					// validate the read that our caller took to get to node
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						return SpecialRetry;
					}

					// At this point we know that the traversal our parent took
					// to get to node is still valid. The recursive
					// implementation will validate the traversal from node to
					// child, so just prior to the nodeOVL validation both
					// traversals were definitely okay. This means that we are
					// no longer vulnerable to node shrinks, and we don't need
					// to validate nodeOVL any more.
					final Object vo = attemptUpdate(key, k, func, expected,
							newValue, node, child, childOVL);
					if (vo != SpecialRetry) {
						return vo;
					}
					// else RETRY
				}
			}
		}
	}

	/**
	 * parent will only be used for unlink, update can proceed even if parent is
	 * stale.
	 */
	private Object attemptNodeUpdate(final int func, final Object expected,
			final Object newValue, final Node<K, V> parent,
			final Node<K, V> node) {
		if (newValue == null) {
			// removal
			if (node.vOpt == null) {
				// This node is already removed, nothing to do.
				return null;
			}
		}

		if (newValue == null && (node.left == null || node.right == null)) {
			// potential unlink, get ready by locking the parent
			final Object prev;
			final Node<K, V> damaged;
			synchronized (parent) {
				if (isUnlinked(parent.changeOVL) || node.parent != parent) {
					return SpecialRetry;
				}

				synchronized (node) {
					prev = node.vOpt;
					if (prev == null || !shouldUpdate(func, prev, expected)) {
						// nothing to do
						return prev;
					}
					if (!attemptUnlink_nl(parent, node)) {
						return SpecialRetry;
					}
				}
				// try to fix the parent while we've still got the lock
				damaged = fixHeight_nl(parent);
			}
			fixHeightAndRebalance(damaged);
			return prev;
		} else {
			// potential update (including remove-without-unlink)
			synchronized (node) {
				// regular version changes don't bother us
				if (isUnlinked(node.changeOVL)) {
					return SpecialRetry;
				}

				final Object prev = node.vOpt;
				if (!shouldUpdate(func, prev, expected)) {
					return prev;
				}

				// retry if we now detect that unlink is possible
				if (newValue == null
						&& (node.left == null || node.right == null)) {
					return SpecialRetry;
				}

				// update in-place
				node.vOpt = newValue;
				return prev;
			}
		}
	}

	/** Does not adjust the size or any heights. */
	private boolean attemptUnlink_nl(final Node<K, V> parent,
			final Node<K, V> node) {
		// assert (Thread.holdsLock(parent));
		// assert (Thread.holdsLock(node));
		assert (!isUnlinked(parent.changeOVL));

		final Node<K, V> parentL = parent.left;
		final Node<K, V> parentR = parent.right;
		if (parentL != node && parentR != node) {
			// node is no longer a child of parent
			return false;
		}

		assert (!isUnlinked(node.changeOVL));
		assert (parent == node.parent);

		final Node<K, V> left = node.left;
		final Node<K, V> right = node.right;
		if (left != null && right != null) {
			// splicing is no longer possible
			return false;
		}
		final Node<K, V> splice = left != null ? left : right;

		if (parentL == node) {
			parent.left = splice;
		} else {
			parent.right = splice;
		}
		if (splice != null) {
			splice.parent = parent;
		}

		node.changeOVL = UnlinkedOVL;
		node.vOpt = null;

		return true;
	}

	// ////////////// NavigableMap stuff

	public Map.Entry<K, V> pollFirstEntry() {
		return pollExtremeEntry(Left);
	}

	public Map.Entry<K, V> pollLastEntry() {
		return pollExtremeEntry(Right);
	}

	private Map.Entry<K, V> pollExtremeEntry(final char dir) {
		while (true) {
			final Node<K, V> right = rootHolder.right;
			if (right == null) {
				// tree is empty, nothing to remove
				return null;
			} else {
				final long ovl = right.changeOVL;
				if (isShrinkingOrUnlinked(ovl)) {
					right.waitUntilChangeCompleted(ovl);
					// RETRY
				} else if (right == rootHolder.right) {
					// this is the protected .right
					final Map.Entry<K, V> result = attemptRemoveExtreme(dir,
							rootHolder, right, ovl);
					if (result != null) {
						return result;
					}
					// else RETRY
				}
			}
		}
	}

	/** Optimistic failure is returned as null. */
	private Map.Entry<K, V> attemptRemoveExtreme(final char dir,
			final Node<K, V> parent, final Node<K, V> node, final long nodeOVL) {
		assert (nodeOVL != UnlinkedOVL);

		while (true) {
			final Node<K, V> child = node.child(dir);

			if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
				return null;
			}

			if (child == null) {
				// potential unlink, get ready by locking the parent
				final Object vo;
				final Node<K, V> damaged;
				synchronized (parent) {
					if (isUnlinked(parent.changeOVL) || node.parent != parent) {
						return null;
					}

					synchronized (node) {
						vo = node.vOpt;
						if (node.child(dir) != null
								|| !attemptUnlink_nl(parent, node)) {
							return null;
						}
						// success!
					}
					// try to fix parent.height while we've still got the lock
					damaged = fixHeight_nl(parent);
				}
				fixHeightAndRebalance(damaged);
				return new SimpleImmutableEntry<K, V>(node.key, decodeNull(vo));
			} else {
				// keep going down
				final long childOVL = child.changeOVL;
				if (isShrinkingOrUnlinked(childOVL)) {
					child.waitUntilChangeCompleted(childOVL);
					// RETRY
				} else if (child != node.child(dir)) {
					// this second read is important, because it is protected
					// by childOVL
					// RETRY
				} else {
					// validate the read that our caller took to get to node
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						return null;
					}

					final Map.Entry<K, V> result = attemptRemoveExtreme(dir,
							node, child, childOVL);
					if (result != null) {
						return result;
					}
					// else RETRY
				}
			}
		}
	}

	// ////////////// tree balance and height info repair

	private static final int UnlinkRequired = -1;
	private static final int RebalanceRequired = -2;
	private static final int NothingRequired = -3;

	private int nodeCondition(final Node<K, V> node) {
		// Begin atomic.

		final Node<K, V> nL = node.left;
		final Node<K, V> nR = node.right;

		if ((nL == null || nR == null) && node.vOpt == null) {
			return UnlinkRequired;
		}

		final int hN = node.height;
		final int hL0 = height(nL);
		final int hR0 = height(nR);

		// End atomic. Since any thread that changes a node promises to fix
		// it, either our read was consistent (and a NothingRequired conclusion
		// is correct) or someone else has taken responsibility for either node
		// or one of its children.

		final int hNRepl = 1 + Math.max(hL0, hR0);
		final int bal = hL0 - hR0;

		if (bal < -1 || bal > 1) {
			return RebalanceRequired;
		}

		return hN != hNRepl ? hNRepl : NothingRequired;
	}

	private void fixHeightAndRebalance(Node<K, V> node) {
		while (node != null && node.parent != null) {
			final int condition = nodeCondition(node);
			if (condition == NothingRequired || isUnlinked(node.changeOVL)) {
				// nothing to do, or no point in fixing this node
				return;
			}

			if (condition != UnlinkRequired && condition != RebalanceRequired) {
				synchronized (node) {
					node = fixHeight_nl(node);
				}
			} else {
				final Node<K, V> nParent = node.parent;
				synchronized (nParent) {
					if (!isUnlinked(nParent.changeOVL)
							&& node.parent == nParent) {
						synchronized (node) {
							node = rebalance_nl(nParent, node);
						}
					}
					// else RETRY
				}
			}
		}
	}

	/**
	 * Attempts to fix the height of a (locked) damaged node, returning the
	 * lowest damaged node for which this thread is responsible. Returns null if
	 * no more repairs are needed.
	 */
	private Node<K, V> fixHeight_nl(final Node<K, V> node) {
		final int c = nodeCondition(node);
		switch (c) {
		case RebalanceRequired:
		case UnlinkRequired:
			// can't repair
			return node;
		case NothingRequired:
			// Any future damage to this node is not our responsibility.
			return null;
		default:
			node.height = c;
			// we've damaged our parent, but we can't fix it now
			return node.parent;
		}
	}

	/**
	 * nParent and n must be locked on entry. Returns a damaged node, or null if
	 * no more rebalancing is necessary.
	 */
	private Node<K, V> rebalance_nl(final Node<K, V> nParent, final Node<K, V> n) {

		final Node<K, V> nL = n.left;
		final Node<K, V> nR = n.right;

		if ((nL == null || nR == null) && n.vOpt == null) {
			if (attemptUnlink_nl(nParent, n)) {
				// attempt to fix nParent.height while we've still got the lock
				return fixHeight_nl(nParent);
			} else {
				// retry needed for n
				return n;
			}
		}

		final int hN = n.height;
		final int hL0 = height(nL);
		final int hR0 = height(nR);
		final int hNRepl = 1 + Math.max(hL0, hR0);
		final int bal = hL0 - hR0;

		if (bal > 1) {
			return rebalanceToRight_nl(nParent, n, nL, hR0);
		} else if (bal < -1) {
			return rebalanceToLeft_nl(nParent, n, nR, hL0);
		} else if (hNRepl != hN) {
			// we've got more than enough locks to do a height change, no need
			// to
			// trigger a retry
			n.height = hNRepl;

			// nParent is already locked, let's try to fix it too
			return fixHeight_nl(nParent);
		} else {
			// nothing to do
			return null;
		}
	}

	private Node<K, V> rebalanceToRight_nl(final Node<K, V> nParent,
			final Node<K, V> n, final Node<K, V> nL, final int hR0) {
		// L is too large, we will rotate-right. If L.R is taller
		// than L.L, then we will first rotate-left L.
		synchronized (nL) {
			final int hL = nL.height;
			if (hL - hR0 <= 1) {
				return n; // retry
			} else {
				final Node<K, V> nLR = nL.right;
				final int hLL0 = height(nL.left);
				final int hLR0 = height(nLR);
				if (hLL0 >= hLR0) {
					// rotate right based on our snapshot of hLR
					return rotateRight_nl(nParent, n, nL, hR0, hLL0, nLR, hLR0);
				} else {
					synchronized (nLR) {
						// If our hLR snapshot is incorrect then we might
						// actually need to do a single rotate-right on n.
						final int hLR = nLR.height;
						if (hLL0 >= hLR) {
							return rotateRight_nl(nParent, n, nL, hR0, hLL0,
									nLR, hLR);
						} else {
							// If the underlying left balance would not be
							// sufficient to actually fix n.left, then instead
							// of rolling it into a double rotation we do it on
							// it's own. This may let us avoid rotating n at
							// all, but more importantly it avoids the creation
							// of damaged nodes that don't have a direct
							// ancestry relationship. The recursive call to
							// rebalanceToRight_nl in this case occurs after we
							// release the lock on nLR.
							final int hLRL = height(nLR.left);
							final int b = hLL0 - hLRL;
							if (b >= -1 && b <= 1) {
								// nParent.child.left won't be damaged after a
								// double rotation
								return rotateRightOverLeft_nl(nParent, n, nL,
										hR0, hLL0, nLR, hLRL);
							}
						}
					}
					// focus on nL, if necessary n will be balanced later
					return rebalanceToLeft_nl(n, nL, nLR, hLL0);
				}
			}
		}
	}

	private Node<K, V> rebalanceToLeft_nl(final Node<K, V> nParent,
			final Node<K, V> n, final Node<K, V> nR, final int hL0) {
		synchronized (nR) {
			final int hR = nR.height;
			if (hL0 - hR >= -1) {
				return n; // retry
			} else {
				final Node<K, V> nRL = nR.left;
				final int hRL0 = height(nRL);
				final int hRR0 = height(nR.right);
				if (hRR0 >= hRL0) {
					return rotateLeft_nl(nParent, n, hL0, nR, nRL, hRL0, hRR0);
				} else {
					synchronized (nRL) {
						final int hRL = nRL.height;
						if (hRR0 >= hRL) {
							return rotateLeft_nl(nParent, n, hL0, nR, nRL, hRL,
									hRR0);
						} else {
							final int hRLR = height(nRL.right);
							final int b = hRR0 - hRLR;
							if (b >= -1 && b <= 1) {
								return rotateLeftOverRight_nl(nParent, n, hL0,
										nR, nRL, hRR0, hRLR);
							}
						}
					}
					return rebalanceToRight_nl(n, nR, nRL, hRR0);
				}
			}
		}
	}

	private Node<K, V> rotateRight_nl(final Node<K, V> nParent,
			final Node<K, V> n, final Node<K, V> nL, final int hR,
			final int hLL, final Node<K, V> nLR, final int hLR) {
		final long nodeOVL = n.changeOVL;
		final long leftOVL = nL.changeOVL;

		if (STRUCT_MODS)
			counts.get().structMods += 1;

		final Node<K, V> nPL = nParent.left;

		n.changeOVL = beginShrink(nodeOVL);
		nL.changeOVL = beginGrow(leftOVL);

		// Down links originally to shrinking nodes should be the last to
		// change,
		// because if we change them early a search might bypass the OVL that
		// indicates its invalidity. Down links originally from shrinking nodes
		// should be the first to change, because we have complete freedom when
		// to
		// change them. s/down/up/ and s/shrink/grow/ for the parent links.

		n.left = nLR;
		nL.right = n;
		if (nPL == n) {
			nParent.left = nL;
		} else {
			nParent.right = nL;
		}

		nL.parent = nParent;
		n.parent = nL;
		if (nLR != null) {
			nLR.parent = n;
		}

		// fix up heights links
		final int hNRepl = 1 + Math.max(hLR, hR);
		n.height = hNRepl;
		nL.height = 1 + Math.max(hLL, hNRepl);

		nL.changeOVL = endGrow(leftOVL);
		n.changeOVL = endShrink(nodeOVL);

		// We have damaged nParent, n (now parent.child.right), and nL (now
		// parent.child). n is the deepest. Perform as many fixes as we can
		// with the locks we've got.

		// We've already fixed the height for n, but it might still be outside
		// our allowable balance range. In that case a simple fixHeight_nl
		// won't help.
		final int balN = hLR - hR;
		if (balN < -1 || balN > 1) {
			// we need another rotation at n
			return n;
		}

		// we've already fixed the height at nL, do we need a rotation here?
		final int balL = hLL - hNRepl;
		if (balL < -1 || balL > 1) {
			return nL;
		}

		// try to fix the parent height while we've still got the lock
		return fixHeight_nl(nParent);
	}

	private Node<K, V> rotateLeft_nl(final Node<K, V> nParent,
			final Node<K, V> n, final int hL, final Node<K, V> nR,
			final Node<K, V> nRL, final int hRL, final int hRR) {
		final long nodeOVL = n.changeOVL;
		final long rightOVL = nR.changeOVL;

		final Node<K, V> nPL = nParent.left;

		if (STRUCT_MODS)
			counts.get().structMods += 1;

		n.changeOVL = beginShrink(nodeOVL);
		nR.changeOVL = beginGrow(rightOVL);

		n.right = nRL;
		nR.left = n;
		if (nPL == n) {
			nParent.left = nR;
		} else {
			nParent.right = nR;
		}

		nR.parent = nParent;
		n.parent = nR;
		if (nRL != null) {
			nRL.parent = n;
		}

		// fix up heights
		final int hNRepl = 1 + Math.max(hL, hRL);
		n.height = hNRepl;
		nR.height = 1 + Math.max(hNRepl, hRR);

		nR.changeOVL = endGrow(rightOVL);
		n.changeOVL = endShrink(nodeOVL);

		final int balN = hRL - hL;
		if (balN < -1 || balN > 1) {
			return n;
		}

		final int balR = hRR - hNRepl;
		if (balR < -1 || balR > 1) {
			return nR;
		}

		return fixHeight_nl(nParent);
	}

	private Node<K, V> rotateRightOverLeft_nl(final Node<K, V> nParent,
			final Node<K, V> n, final Node<K, V> nL, final int hR,
			final int hLL, final Node<K, V> nLR, final int hLRL) {
		final long nodeOVL = n.changeOVL;
		final long leftOVL = nL.changeOVL;
		final long leftROVL = nLR.changeOVL;

		final Node<K, V> nPL = nParent.left;
		final Node<K, V> nLRL = nLR.left;
		final Node<K, V> nLRR = nLR.right;
		final int hLRR = height(nLRR);

		if (STRUCT_MODS)
			counts.get().structMods += 1;

		n.changeOVL = beginShrink(nodeOVL);
		nL.changeOVL = beginShrink(leftOVL);
		nLR.changeOVL = beginGrow(leftROVL);

		n.left = nLRR;
		nL.right = nLRL;
		nLR.left = nL;
		nLR.right = n;
		if (nPL == n) {
			nParent.left = nLR;
		} else {
			nParent.right = nLR;
		}

		nLR.parent = nParent;
		nL.parent = nLR;
		n.parent = nLR;
		if (nLRR != null) {
			nLRR.parent = n;
		}
		if (nLRL != null) {
			nLRL.parent = nL;
		}

		// fix up heights
		final int hNRepl = 1 + Math.max(hLRR, hR);
		n.height = hNRepl;
		final int hLRepl = 1 + Math.max(hLL, hLRL);
		nL.height = hLRepl;
		nLR.height = 1 + Math.max(hLRepl, hNRepl);

		nLR.changeOVL = endGrow(leftROVL);
		nL.changeOVL = endShrink(leftOVL);
		n.changeOVL = endShrink(nodeOVL);

		// caller should have performed only a single rotation if nL was going
		// to end up damaged
		assert (Math.abs(hLL - hLRL) <= 1);

		// We have damaged nParent, nLR (now parent.child), and n (now
		// parent.child.right). n is the deepest. Perform as many fixes as we
		// can with the locks we've got.

		// We've already fixed the height for n, but it might still be outside
		// our allowable balance range. In that case a simple fixHeight_nl
		// won't help.
		final int balN = hLRR - hR;
		if (balN < -1 || balN > 1) {
			// we need another rotation at n
			return n;
		}

		// we've already fixed the height at nLR, do we need a rotation here?
		final int balLR = hLRepl - hNRepl;
		if (balLR < -1 || balLR > 1) {
			return nLR;
		}

		// try to fix the parent height while we've still got the lock
		return fixHeight_nl(nParent);
	}

	private Node<K, V> rotateLeftOverRight_nl(final Node<K, V> nParent,
			final Node<K, V> n, final int hL, final Node<K, V> nR,
			final Node<K, V> nRL, final int hRR, final int hRLR) {
		final long nodeOVL = n.changeOVL;
		final long rightOVL = nR.changeOVL;
		final long rightLOVL = nRL.changeOVL;

		final Node<K, V> nPL = nParent.left;
		final Node<K, V> nRLL = nRL.left;
		final int hRLL = height(nRLL);
		final Node<K, V> nRLR = nRL.right;

		if (STRUCT_MODS)
			counts.get().structMods += 1;

		n.changeOVL = beginShrink(nodeOVL);
		nR.changeOVL = beginShrink(rightOVL);
		nRL.changeOVL = beginGrow(rightLOVL);

		n.right = nRLL;
		nR.left = nRLR;
		nRL.right = nR;
		nRL.left = n;
		if (nPL == n) {
			nParent.left = nRL;
		} else {
			nParent.right = nRL;
		}

		nRL.parent = nParent;
		nR.parent = nRL;
		n.parent = nRL;
		if (nRLL != null) {
			nRLL.parent = n;
		}
		if (nRLR != null) {
			nRLR.parent = nR;
		}

		// fix up heights
		final int hNRepl = 1 + Math.max(hL, hRLL);
		n.height = hNRepl;
		final int hRRepl = 1 + Math.max(hRLR, hRR);
		nR.height = hRRepl;
		nRL.height = 1 + Math.max(hNRepl, hRRepl);

		nRL.changeOVL = endGrow(rightLOVL);
		nR.changeOVL = endShrink(rightOVL);
		n.changeOVL = endShrink(nodeOVL);

		assert (Math.abs(hRR - hRLR) <= 1);

		final int balN = hRLL - hL;
		if (balN < -1 || balN > 1) {
			return n;
		}
		final int balRL = hRRepl - hNRepl;
		if (balRL < -1 || balRL > 1) {
			return nRL;
		}
		return fixHeight_nl(nParent);
	}

	// ////////////// iteration (node successor)

	@SuppressWarnings("unchecked")
	private Node<K, V> firstNode() {
		return (Node<K, V>) extreme(ReturnNode, Left);
	}

	/** Returns the successor to a node, or null if no successor exists. */
	@SuppressWarnings("unchecked")
	private Node<K, V> succ(final Node<K, V> node) {
		while (true) {
			final Object z = attemptSucc(node);
			if (z != SpecialRetry) {
				return (Node<K, V>) z;
			}
		}
	}

	private Object attemptSucc(final Node<K, V> node) {
		if (isUnlinked(node.changeOVL)) {
			return succOfUnlinked(node);
		}

		final Node<K, V> right = node.right;
		if (right != null) {
			// If right undergoes a right rotation then its first won't be our
			// successor. We need to recheck node.right after guaranteeing that
			// right can't shrink. We actually don't care about shrinks or grows
			// of
			// node itself once we've gotten hold of a right node.
			final long rightOVL = right.changeOVL;
			if (isShrinkingOrUnlinked(rightOVL)) {
				right.waitUntilChangeCompleted(rightOVL);
				return SpecialRetry;
			}

			if (node.right != right) {
				return SpecialRetry;
			}

			return attemptExtreme(ReturnNode, Left, right, rightOVL);
		} else {
			final long nodeOVL = node.changeOVL;
			if (isChangingOrUnlinked(nodeOVL)) {
				node.waitUntilChangeCompleted(nodeOVL);
				return SpecialRetry;
			}

			// This check of node.right is the one that is protected by the
			// nodeOVL
			// check in succUp().
			if (node.right != null) {
				return SpecialRetry;
			}

			return succUp(node, nodeOVL);
		}
	}

	private Object succUp(final Node<K, V> node, final long nodeOVL) {
		if (node == rootHolder) {
			return null;
		}

		while (true) {
			final Node<K, V> parent = node.parent;
			final long parentOVL = parent.changeOVL;
			if (isChangingOrUnlinked(parentOVL)) {
				parent.waitUntilChangeCompleted(parentOVL);

				if (hasChangedOrUnlinked(nodeOVL, node.changeOVL)) {
					return SpecialRetry;
				}
				// else just RETRY at this level
			} else if (node == parent.left) {
				// This check validates the caller's test in which node.right
				// was not
				// an adequate successor. In attemptSucc that test is
				// .right==null, in
				// our recursive parent that test is parent.right==node.
				if (hasChangedOrUnlinked(nodeOVL, node.changeOVL)) {
					return SpecialRetry;
				}

				// Parent is the successor. We don't care whether or not the
				// parent
				// has grown, because we know that we haven't and there aren't
				// any
				// nodes in between the parent and ourself.
				return parent;
			} else if (node != parent.right) {
				if (hasChangedOrUnlinked(nodeOVL, node.changeOVL)) {
					return SpecialRetry;
				}
				// else RETRY at this level
			} else {
				// This is the last check of node.changeOVL (unless the parent
				// fails). After this point we are immune to growth of node.
				if (hasChangedOrUnlinked(nodeOVL, node.changeOVL)) {
					return SpecialRetry;
				}

				final Object z = succUp(parent, parentOVL);
				if (z != SpecialRetry) {
					return z;
				}
				// else RETRY at this level
			}
		}
	}

	/** Returns the successor to an unlinked node. */
	private Object succOfUnlinked(final Node<K, V> node) {
		return succNode(node.key);
	}

	private Object succNode(final K key) {
		final Comparable<? super K> keyCmp = comparable(key);

		while (true) {
			final Node<K, V> right = rootHolder.right;
			if (right == null) {
				return null;
			}

			final long ovl = right.changeOVL;
			if (isShrinkingOrUnlinked(ovl)) {
				right.waitUntilChangeCompleted(ovl);
				// RETRY
			} else if (right == rootHolder.right) {
				// note that the protected read of root.right is actually the
				// one in
				// the if(), not the read that initialized right
				final Object z = succNode(keyCmp, right, ovl);
				if (z != SpecialRetry) {
					return z;
				}
				// else RETRY
			}
		}
	}

	private Object succNode(final Comparable<? super K> keyCmp,
			final Node<K, V> node, final long nodeOVL) {
		while (true) {
			final int cmp = keyCmp.compareTo(node.key);

			if (cmp >= 0) {
				// node.key <= keyCmp, so succ is on right branch
				final Node<K, V> right = node.right;
				if (right == null) {
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						return SpecialRetry;
					}
					return null;
				} else {
					final long rightOVL = right.changeOVL;
					if (isShrinkingOrUnlinked(rightOVL)) {
						right.waitUntilChangeCompleted(rightOVL);

						if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
							return SpecialRetry;
						}
						// else RETRY
					} else if (right != node.right) {
						// this second read is important, because it is
						// protected by rightOVL

						if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
							return SpecialRetry;
						}
						// else RETRY
					} else {
						if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
							return SpecialRetry;
						}

						final Object z = succNode(keyCmp, right, rightOVL);
						if (z != SpecialRetry) {
							return z;
						}
						// else RETRY
					}
				}
			} else {
				// succ is either on the left branch or is node
				final Node<K, V> left = node.left;
				if (left == null) {
					if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
						return SpecialRetry;
					}

					return node;
				} else {
					final long leftOVL = left.changeOVL;
					if (isShrinkingOrUnlinked(leftOVL)) {
						left.waitUntilChangeCompleted(leftOVL);

						if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
							return SpecialRetry;
						}
						// else RETRY at this level
					} else if (left != node.left) {
						if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
							return SpecialRetry;
						}
						// else RETRY at this level
					} else {
						if (hasShrunkOrUnlinked(nodeOVL, node.changeOVL)) {
							return SpecialRetry;
						}
						final Object z = succNode(keyCmp, left, leftOVL);
						if (z != SpecialRetry) {
							return z == null ? node : z;
						}
						// else RETRY
					}
				}
			}
		}
	}

	// ////////////// views

	@Override
	public Set<Map.Entry<K, V>> entrySet() {
		return entries;
	}

	private class EntrySet extends AbstractSet<Entry<K, V>> {

		@Override
		public int size() {
			return LockBasedStanfordTreeMap.this.size();
		}

		@Override
		public boolean isEmpty() {
			return LockBasedStanfordTreeMap.this.isEmpty();
		}

		@Override
		public void clear() {
			LockBasedStanfordTreeMap.this.clear();
		}

		@Override
		public boolean contains(final Object o) {
			if (!(o instanceof Entry<?, ?>)) {
				return false;
			}
			final Object k = ((Entry<?, ?>) o).getKey();
			final Object v = ((Entry<?, ?>) o).getValue();
			final Object actualVo = LockBasedStanfordTreeMap.this.getImpl(k);
			if (actualVo == null) {
				// no associated value
				return false;
			}
			final V actual = decodeNull(actualVo);
			return v == null ? actual == null : v.equals(actual);
		}

		@Override
		public boolean add(final Entry<K, V> e) {
			final Object v = encodeNull(e.getValue());
			return update(e.getKey(), UpdateAlways, null, v) != v;
		}

		@Override
		public boolean remove(final Object o) {
			if (!(o instanceof Entry<?, ?>)) {
				return false;
			}
			final Object k = ((Entry<?, ?>) o).getKey();
			final Object v = ((Entry<?, ?>) o).getValue();
			return LockBasedStanfordTreeMap.this.remove(k, v);
		}

		@Override
		public Iterator<Entry<K, V>> iterator() {
			return new EntryIter();
		}
	}

	private class EntryIter implements Iterator<Entry<K, V>> {
		private Node<K, V> cp;
		private SimpleImmutableEntry<K, V> availEntry;
		private Node<K, V> availNode;
		private Node<K, V> mostRecentNode;

		EntryIter() {
			cp = firstNode();
			advance();
		}

		private void advance() {
			while (cp != null) {
				final K k = cp.key;
				final Object vo = cp.vOpt;
				availNode = cp;
				cp = succ(cp);
				if (vo != null) {
					availEntry = new SimpleImmutableEntry<K, V>(k,
							decodeNull(vo));
					return;
				}
			}
			availEntry = null;
		}

		@Override
		public boolean hasNext() {
			return availEntry != null;
		}

		@Override
		public Map.Entry<K, V> next() {
			mostRecentNode = availNode;
			final Map.Entry<K, V> z = availEntry;
			advance();
			return z;
		}

		@Override
		public void remove() {
			LockBasedStanfordTreeMap.this.remove(mostRecentNode.key);
		}
	}
}
