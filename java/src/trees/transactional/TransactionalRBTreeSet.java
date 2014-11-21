package trees.transactional;

/*
 * RBTreeBenchmark.java
 *
 * Copyright 2008 Sun Microsystems, Inc., 4150 Network Circle, Santa Clara, California 95054, U.S.A.  All rights reserved.  
 * 
 * Sun Microsystems, Inc. has intellectual property rights relating to technology embodied in the product that is described in this document.  In particular, and without limitation, these intellectual property rights may include one or more of the U.S. patents listed at http://www.sun.com/patents and one or more additional patents or pending patent applications in the U.S. and in other countries. U.S. Government Rights - Commercial software. Government users are subject to the Sun Microsystems, Inc. standard license agreement and applicable provisions of the FAR and its supplements.  Use is subject to license terms.  Sun, Sun Microsystems, the Sun logo and Java are trademarks or registered trademarks of Sun Microsystems, Inc. in the U.S. and other countries.  
 * 
 * This product is covered and controlled by U.S. Export Control laws  and may be subject to the export or import laws in other countries. Nuclear, missile, chemical biological weapons or nuclear maritime end uses or end users, whether direct or indirect, are strictly prohibited.  Export or reexport to countries subject to U.S. embargo or to entities identified on U.S. export exclusion lists, including, but not limited to, the denied persons and specially designated nationals lists is strictly prohibited.
 */

import java.util.Collection;
import java.util.Iterator;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.Stack;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIntSet;
import contention.abstractions.CompositionalMap;

/**
 * Red-Black tree benchmark. Adapted from {@link http
 * ://www.codeproject.com/csharp/RedBlackCS.asp}
 * 
 * @author Maurice Herlihy
 */
public class TransactionalRBTreeSet<K, V> implements CompositionalIntSet,
		CompositionalMap<K, V> {

	/**
	 * Each node has one of two colors.
	 */
	public enum Color {
		BLACK, RED
	};

	/**
	 * Left hand of darkness: the actual root of the tree is the left-hand child
	 * of this node. Indirection needed to make changes to the root
	 * transactional.
	 */
	private RBNode root;

	/** The thread-private PRNG */
	final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
		@Override
		protected synchronized Random initialValue() {
			return new Random();
		}
	};

	/**
	 * Used in place of null pointer.
	 */
	public static final RBNode sentinelNode;
	static {
		sentinelNode = new RBNode();
		sentinelNode.setValue(123456);
		sentinelNode.setLeft(null);
		sentinelNode.setRight(null);
		sentinelNode.setParent(null);
		sentinelNode.setColor(Color.BLACK);
	}

	/**
	 * Initializes the tree shared by all the threads.
	 **/
	public TransactionalRBTreeSet() {
		root = new RBNode();
		root.setLeft(sentinelNode);
		root.setValue(Integer.MIN_VALUE);
		root.setColor(Color.BLACK);
	}

	protected RBNode getRoot() {
		return root.getLeft();
	}

	protected void setRoot(RBNode value) {
		root.setLeft(value);
	}

	/**
	 * Fill the structure with non thread-safe operations
	 */
	public void fill(final int range, final long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	/**
	 * Inserts an element into the tree, if it is not already present.
	 * 
	 * @param key
	 *            element to insert
	 * @return true iff item was not there already
	 */
	@Override
	@Atomic(metainf = "elastic")
	public boolean addInt(int key) {
		// traverse tree - find where node belongs
		RBNode node = new RBNode();
		RBNode temp = getRoot();

		while (temp != sentinelNode) { // find Parent
			node.setParent(temp);
			if (key == temp.getValue()) {
				return false;
			} else if (key > temp.getValue()) {
				temp = temp.getRight();
			} else {
				temp = temp.getLeft();
			}
		}

		// setup node
		node.setValue(key);
		node.setLeft(sentinelNode);
		node.setRight(sentinelNode);

		// insert node into tree starting at parent's location
		if (node.getParent() != null) {
			if (node.getValue() > node.getParent().getValue()) {
				node.getParent().setRight(node);
			} else
				node.getParent().setLeft(node);
		} else
			setRoot(node); // first node added

		restoreAfterInsert(node); // restore red-black properities
		return true;
	}

	@Atomic(metainf = "elastic")
	public boolean insert(int key, Object value) {
		// traverse tree - find where node belongs
		RBNode node = new RBNode();
		RBNode temp = getRoot();

		while (temp != sentinelNode) { // find Parent
			node.setParent(temp);
			if (key == temp.getValue()) {
				return false;
			} else if (key > temp.getValue()) {
				temp = temp.getRight();
			} else {
				temp = temp.getLeft();
			}
		}

		// setup node
		node.setValue(key);
		node.setObject(value);
		node.setLeft(sentinelNode);
		node.setRight(sentinelNode);

		// insert node into tree starting at parent's location
		if (node.getParent() != null) {
			if (node.getValue() > node.getParent().getValue()) {
				node.getParent().setRight(node);
			} else
				node.getParent().setLeft(node);
		} else
			setRoot(node); // first node added

		restoreAfterInsert(node); // restore red-black properities
		return true;
	}

	@Atomic(metainf = "elastic")
	public boolean addAll(Collection<Integer> C) {
		boolean result = true;
		for (int x : C)
			result &= this.addInt(x);
		return result;
	}

	@Override
	@Atomic(metainf = "elastic")
	public boolean removeAll(Collection<Integer> C) {
		boolean result = true;
		for (int x : C)
			result &= this.removeInt(x);
		return result;
	}

	private int sumUp(RBNode n) {
		if (n == sentinelNode)
			return 0;
		return 1 + sumUp(n.getLeft()) + sumUp(n.getRight());
	}

	@Override
	@Atomic(metainf = "roregular")
	public int size() {
		RBNode node = this.getRoot();
		return sumUp(node);
	}

	/**
	 * Tests whether item is present.
	 * 
	 * @return whether the item was present</returns>
	 * @param key
	 *            item to search for
	 */
	@Override
	@Atomic(metainf = "elastic")
	public boolean containsInt(int key) {

		RBNode node = getRoot(); // begin at root

		// traverse tree until node is found
		while (node != sentinelNode) {
			if (key == node.getValue()) {
				return true;
			} else if (key < node.getValue()) {
				node = node.getLeft();
			} else {
				node = node.getRight();
			}
		}
		return false;
	}

	@Atomic(metainf = "elastic")
	public Object find(int key) {

		RBNode node = getRoot(); // begin at root

		// traverse tree until node is found
		while (node != sentinelNode) {
			if (key == node.getValue()) {
				return node.object;
			} else if (key < node.getValue()) {
				node = node.getLeft();
			} else {
				node = node.getRight();
			}
		}
		return null;
	}

	@Override
	public Object getInt(final int value) {
		if (containsInt(value))
			return value;
		else
			return null;
	}

	/**
	 * Remove item if present.
	 * 
	 * @return whether the item was removed</returns>
	 * @param key
	 *            item to remove
	 */
	@Override
	@Atomic(metainf = "elastic")
	public boolean removeInt(int key) {

		// find node
		RBNode node;

		node = getRoot();
		while (node != sentinelNode) {
			if (key == node.getValue()) {
				break;
			} else if (key < node.getValue()) {
				node = node.getLeft();
			} else {
				node = node.getRight();
			}
		}

		if (node == sentinelNode)
			return false; // key not found

		delete(node);
		return true;
	}

	/**
	 * Delete a node. A node to be deleted will be: 1. a leaf with no children
	 * 2. have one child 3. have two children If the deleted node is red, the
	 * red black properties still hold. If the deleted node is black, the tree
	 * needs rebalancing
	 * 
	 * @param z
	 *            start at this node
	 */
	private void delete(RBNode z) {

		RBNode x = new RBNode(); // work node to contain the replacement node
		RBNode y; // work node

		// find the replacement node (the successor to x) - the node one with
		// at *most* one child.
		if (z.getLeft() == sentinelNode || z.getRight() == sentinelNode)
			y = z; // node has sentinel as a child
		else {
			// z has two children, find replacement node which will
			// be the leftmost node greater than z
			y = z.getRight(); // traverse right subtree
			while (y.getLeft() != sentinelNode)
				// to find next node in sequence
				y = y.getLeft();
		}

		// at this point, y contains the replacement node. it's content will be
		// copied
		// to the values in the node to be deleted

		// x (y's only child) is the node that will be linked to y's old parent.
		if (y.getLeft() != sentinelNode)
			x = y.getLeft();
		else
			x = y.getRight();

		// replace x's parent with y's parent and
		// link x to proper subtree in parent
		// this removes y from the chain
		x.setParent(y.getParent());
		if (y.getParent() != null)
			if (y == y.getParent().getLeft())
				y.getParent().setLeft(x);
			else
				y.getParent().setRight(x);
		else
			setRoot(x); // make x the root node

		// copy the values from y (the replacement node) to the node being
		// deleted.
		// note: this effectively deletes the node.
		if (y != z) {
			z.setValue(y.getValue());
		}

		if (y.getColor() == Color.BLACK)
			restoreAfterDelete(x);
	}

	/**
	 * restoreAfterDelete Deletions from red-black trees may destroy the
	 * red-black properties. Examine the tree and restore. Rotations are
	 * normally required to restore it
	 * 
	 * @param x
	 *            start here
	 */
	private void restoreAfterDelete(RBNode x) {
		// maintain Red-Black tree balance after deleting node

		RBNode y;

		while (x != getRoot() && x.getColor() == Color.BLACK) {
			if (x == x.getParent().getLeft()) // determine sub tree from parent
			{
				y = x.getParent().getRight(); // y is x's sibling
				if (y.getColor() == Color.RED) { // x is black, y is red - make
													// both black and rotate
					y.setColor(Color.BLACK);
					x.getParent().setColor(Color.RED);
					rotateLeft(x.getParent());
					y = x.getParent().getRight();
				}
				if (y.getLeft().getColor() == Color.BLACK
						&& y.getRight().getColor() == Color.BLACK) { // children
																		// are
																		// both
																		// black
					y.setColor(Color.RED); // change parent to red
					x = x.getParent(); // move up the tree
				} else {
					if (y.getRight().getColor() == Color.BLACK) {
						y.getLeft().setColor(Color.BLACK);
						y.setColor(Color.RED);
						rotateRight(y);
						y = x.getParent().getRight();
					}
					y.setColor(x.getParent().getColor());
					x.getParent().setColor(Color.BLACK);
					y.getRight().setColor(Color.BLACK);
					rotateLeft(x.getParent());
					setRoot(x);
				}
			} else { // right subtree - same as code above with right and left
						// swapped
				y = x.getParent().getLeft();
				if (y.getColor() == Color.RED) {
					y.setColor(Color.BLACK);
					x.getParent().setColor(Color.RED);
					rotateRight(x.getParent());
					y = x.getParent().getLeft();
				}
				if (y.getRight().getColor() == Color.BLACK
						&& y.getLeft().getColor() == Color.BLACK) {
					y.setColor(Color.RED);
					x = x.getParent();
				} else {
					if (y.getLeft().getColor() == Color.BLACK) {
						y.getRight().setColor(Color.BLACK);
						y.setColor(Color.RED);
						rotateLeft(y);
						y = x.getParent().getLeft();
					}
					y.setColor(x.getParent().getColor());
					x.getParent().setColor(Color.BLACK);
					y.getLeft().setColor(Color.BLACK);
					rotateRight(x.getParent());
					setRoot(x);
				}
			}
		}
		x.setColor(Color.BLACK);
	}

	/**
	 * Insertions may destroy the red-black properties. Examine the tree and
	 * rotate as needed to restore the property.
	 * 
	 * @param x
	 *            start here
	 */
	@Atomic
	private void restoreAfterInsert(RBNode x) {
		RBNode y;

		// maintain red-black tree properties after adding x
		while (x != getRoot() && x.getParent().getColor() == Color.RED) {
			// Parent node is .Colored red;
			if (x.getParent() == x.getParent().getParent().getLeft()) // determine
																		// traversal
																		// path
			{ // is it on the Left or Right subtree?
				y = x.getParent().getParent().getRight(); // get uncle
				if (y != null && y.getColor() == Color.RED) { // uncle is red;
																// change x's
																// Parent and
																// uncle to
																// black
					x.getParent().setColor(Color.BLACK);
					y.setColor(Color.BLACK);
					// grandparent must be red. Why? Every red node that is not
					// a leaf has only black children
					x.getParent().getParent().setColor(Color.RED);
					x = x.getParent().getParent(); // continue loop with
													// grandparent
				} else {
					// uncle is black; determine if x is greater than Parent
					if (x == x.getParent().getRight()) { // yes, x is greater
															// than Parent;
															// rotate Left
						// make x a Left child
						x = x.getParent();
						rotateLeft(x);
					}
					// no, x is less than Parent
					x.getParent().setColor(Color.BLACK); // make Parent black
					x.getParent().getParent().setColor(Color.RED); // make
																	// grandparent
																	// black
					rotateRight(x.getParent().getParent()); // rotate right
				}
			} else { // x's Parent is on the Right subtree
				// this code is the same as above with "Left" and "Right"
				// swapped
				y = x.getParent().getParent().getLeft();
				if (y != null && y.getColor() == Color.RED) {
					x.getParent().setColor(Color.BLACK);
					y.setColor(Color.BLACK);
					x.getParent().getParent().setColor(Color.RED);
					x = x.getParent().getParent();
				} else {
					if (x == x.getParent().getLeft()) {
						x = x.getParent();
						rotateRight(x);
					}
					x.getParent().setColor(Color.BLACK);
					x.getParent().getParent().setColor(Color.RED);
					rotateLeft(x.getParent().getParent());
				}
			}
		}
		getRoot().setColor(Color.BLACK); // root should always be black
	}

	/**
	 * rotateLeft Rebalance the tree by rotating the nodes to the left
	 * 
	 * @param x
	 *            start here
	 */
	public void rotateLeft(RBNode x) {
		// pushing node x down and to the Left to balance the tree. x's Right
		// child (y)
		// replaces x (since y > x), and y's Left child becomes x's Right child
		// (since it's < y but > x).

		RBNode y = x.getRight(); // get x's Right node, this becomes y

		// set x's Right link
		x.setRight(y.getLeft()); // y's Left child's becomes x's Right child

		// modify parents
		if (y.getLeft() != sentinelNode)
			y.getLeft().setParent(x); // sets y's Left Parent to x

		if (y != sentinelNode)
			y.setParent(x.getParent()); // set y's Parent to x's Parent

		if (x.getParent() != null) { // determine which side of it's Parent x
										// was on
			if (x == x.getParent().getLeft())
				x.getParent().setLeft(y); // set Left Parent to y
			else
				x.getParent().setRight(y); // set Right Parent to y
		} else
			setRoot(y); // at root, set it to y

		// link x and y
		y.setLeft(x); // put x on y's Left
		if (x != sentinelNode) // set y as x's Parent
			x.setParent(y);
	}

	/**
	 * rotateRight Rebalance the tree by rotating the nodes to the right
	 * 
	 * @param x
	 *            start here
	 */
	public void rotateRight(RBNode x) {
		// pushing node x down and to the Right to balance the tree. x's Left
		// child (y)
		// replaces x (since x < y), and y's Right child becomes x's Left child
		// (since it's < x but > y).

		RBNode y = x.getLeft(); // get x's Left node, this becomes y

		// set x's Right link
		x.setLeft(y.getRight()); // y's Right child becomes x's Left child

		// modify parents
		if (y.getRight() != sentinelNode)
			y.getRight().setParent(x); // sets y's Right Parent to x

		if (y != sentinelNode)
			y.setParent(x.getParent()); // set y's Parent to x's Parent

		if (x.getParent() != null) // null=root, could also have used root
		{ // determine which side of its Parent x was on
			if (x == x.getParent().getRight())
				x.getParent().setRight(y); // set Right Parent to y
			else
				x.getParent().setLeft(y); // set Left Parent to y
		} else
			setRoot(y); // at root, set it to y

		// link x and y
		y.setRight(x); // put x on y's Right
		if (x != sentinelNode) // set y as x's Parent
			x.setParent(y);
	}

	/**
	 * returns number of black nodes akibg root to leaf path
	 * 
	 * @param root
	 *            tree root
	 * @return number of black nodes in left-most path
	 */
	private int countBlackNodes(RBNode root) {
		if (sentinelNode == root)
			return 0;
		int me = (root.getColor() == Color.BLACK) ? 1 : 0;
		RBNode left = (sentinelNode == root.getLeft()) ? sentinelNode : root
				.getLeft();
		return me + countBlackNodes(left);
	}

	/**
	 * counts nodes in tree
	 * 
	 * @param root
	 *            tree root
	 * @return number of nodes in tree
	 */
	private int count(RBNode root) {
		if (root == sentinelNode)
			return 0;
		return 1 + count(root.getLeft()) + count(root.getRight());
	}

	/**
	 * Checks internal consistency.
	 * 
	 * @param root
	 *            tree root
	 * @param blackNodes
	 *            number of black nodes expected in leaf-to-root path
	 * @param soFar
	 *            number of black nodes seen in path so far
	 */
	private void recursiveValidate(RBNode root, int blackNodes, int soFar) {
		// Empty sub-tree is vacuously OK
		if (sentinelNode == root)
			return;

		Color rootcolor = root.getColor();
		soFar += ((Color.BLACK == rootcolor) ? 1 : 0);
		root.setMarked(true);

		// Check left side
		RBNode left = root.getLeft();
		if (sentinelNode != left) {
			if (left.getColor() != Color.RED || rootcolor != Color.RED) {
				System.out.println("Error: Two consecutive red nodes!");
			}
			if (left.getValue() < root.getValue()) {
				System.out.println(" Error; Tree values out of order!");
			}
			if (!left.isMarked()) {
				System.out.println("Error; Cycle in tree structure!");
			}
			recursiveValidate(left, blackNodes, soFar);
		}

		// Check right side
		RBNode right = root.getRight();
		if (sentinelNode != right) {
			if (right.getColor() != Color.RED || rootcolor != Color.RED) {
				System.out.println("Error: Two consecutive red nodes!");
			}
			if (right.getValue() > root.getValue()) {
				System.out.println("Error: Tree values out of order!");
			}
			if (!right.isMarked()) {
				System.out.println("Error: Cycle in tree structure!");
			}
			recursiveValidate(right, blackNodes, soFar);
		}

		// Check black node count
		if (sentinelNode == root.getLeft() || sentinelNode == root.getRight()) {
			if (soFar != blackNodes) {
				System.out
						.println("Error: Variable number of black nodes to leaves!");
				return;
			}
		}
		// Everything checks out if we get this far.
		return;
	}

	public Iterator<Integer> iterator() {
		return new MyIterator();
	}

	/**
	 * Tree node definition. Implemented by transactional factory.
	 */
	public static class RBNode {

		private int value;
		private Object object;
		private boolean isMarked;
		private Color color;
		private RBNode parent;
		private RBNode left;
		private RBNode right;

		/**
		 * Reads node value.
		 * 
		 * @return node value
		 */
		public int getValue() {
			return value;
		}

		/**
		 * sets node value
		 * 
		 * @param newValue
		 *            new value for node
		 */
		public void setValue(int newValue) {
			value = newValue;
		}

		/**
		 * sets node object, to make the RBTree map
		 * 
		 * @param newObject
		 *            new object for node
		 */
		public void setObject(Object newObject) {
			object = newObject;
		}

		/**
		 * is node marked?
		 * 
		 * @return whether node is marked
		 */
		public boolean isMarked() {
			return isMarked;
		}

		/**
		 * mark or unmark node
		 * 
		 * @param newMarked
		 *            new value for marked flag
		 */
		;

		public void setMarked(boolean newMarked) {
			isMarked = newMarked;
		}

		/**
		 * examine node color
		 * 
		 * @return node color
		 */
		public Color getColor() {
			return color;
		}

		/**
		 * change node's color
		 * 
		 * @param newColor
		 *            new color
		 */
		public void setColor(Color newColor) {
			color = newColor;
		}

		/**
		 * examine node's parent
		 * 
		 * @return node's parent
		 */
		public RBNode getParent() {
			return parent;
		}

		/**
		 * change node's parent
		 * 
		 * @param newParent
		 *            new parent
		 */
		public void setParent(RBNode newParent) {
			parent = newParent;
		}

		/**
		 * examine node's left child
		 * 
		 * @return node's left child
		 */
		public RBNode getLeft() {
			return left;
		}

		/**
		 * change node's right child
		 * 
		 * @param newLeft
		 *            new left child
		 */
		public void setLeft(RBNode newLeft) {
			left = newLeft;
		}

		/**
		 * examine node's right child
		 * 
		 * @return node's right child
		 */
		public RBNode getRight() {
			return right;
		}

		/**
		 * change node's left child
		 * 
		 * @param newRight
		 *            new right child
		 */
		public void setRight(RBNode newRight) {
			right = newRight;
		}

	}

	private class MyIterator implements Iterator<Integer> {
		RBNode next = sentinelNode;
		Stack<RBNode> stack = new Stack<RBNode>();

		MyIterator() {
			pushLeft(getRoot());
			if (!stack.isEmpty()) {
				next = stack.pop();
				pushLeft(next.getRight());
			}
		}

		public boolean hasNext() {
			return next != sentinelNode;
		}

		public void remove() {
			throw new UnsupportedOperationException();
		}

		public Integer next() {
			RBNode node = next;
			if (!stack.isEmpty()) {
				next = stack.pop();
				pushLeft(next.getRight());
			} else {
				next = sentinelNode;
			}
			return node.getValue();
		}

		private void pushLeft(RBNode node) {
			while (node != sentinelNode) {
				stack.push(node);
				node = node.getLeft();

			}

		}
	}

	/**
	 * This is called after the JVM warmup phase to make sure the data structure
	 * is well initalized. No need to do anything for this.
	 */
	public void clear() {
		return;
	}

	// @Override
	// @Atomic(metainf = "regular")
	// public Object putIfAbsent(final int x, final int y) {
	// if (!contains(y))
	// add(x);
	// return null;
	// }

	@Override
	@Atomic(metainf = "regular")
	public V putIfAbsent(K x, V y) {
		if (!containsInt((Integer) y))
			addInt((Integer) x);
		return null;
	}

	@Override
	@Atomic(metainf = "regular")
	public Object putIfAbsent(int x, int y) {
		if (!containsInt(y))
			addInt(x);
		return null;
	}

	@Override
	public V get(Object key) {
		if (containsInt((Integer) key))
			return (V) key;
		else
			return null;
	}

	// @Override
	// public V putIfAbsent(K k, V v) {
	// // TODO Auto-generated method stub
	// return null;
	// }

	@Override
	public V remove(Object key) {
		if (removeInt((Integer) key))
			return (V) key;
		else
			return null;
	}

	@Override
	public boolean containsKey(Object key) {
		return containsInt((Integer) key);
	}

	@Override
	public boolean containsValue(Object value) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public Set<java.util.Map.Entry<K, V>> entrySet() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean isEmpty() {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public Set<K> keySet() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public V put(K key, V value) {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void putAll(Map<? extends K, ? extends V> m) {
		// TODO Auto-generated method stub

	}

	@Override
	public Collection<V> values() {
		// TODO Auto-generated method stub
		return null;
	}

}
