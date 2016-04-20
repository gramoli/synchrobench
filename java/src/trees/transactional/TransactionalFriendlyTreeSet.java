package trees.transactional;

import java.util.Collection;
import java.util.Map;
import java.util.Random;
import java.util.Set;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIntSet;
import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;

/**
 * A Java implementation of the Speculation-friendly binary search tree
 * as described in:
 *
 * T. Crain, V. Gramoli and M.Raynal. A Speculation-friendly Binary 
 * Search Tree. PPoPP 2012.
 * 
 * @author Tyler Crain
 * 
 */
public class TransactionalFriendlyTreeSet<K, V> implements CompositionalIntSet,
		CompositionalMap<K, V>, MaintenanceAlg {

	/** The thread-private PRNG */
	final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
		@Override
		protected synchronized Random initialValue() {
			return new Random();
		}
	};

	private class MaintenanceThread extends Thread {
		TransactionalFriendlyTreeSet map;

		MaintenanceThread(TransactionalFriendlyTreeSet map) {
			this.map = map;
		}

		public void run() {
			map.doMaintenance();
		}
	}

	private static class Node {
		int key;
		int localh, lefth, righth;
		Node left;
		Node right;
		boolean deleted;

		Node(final int key) {
			this.key = key;
			this.deleted = false;
			this.right = null;
			this.left = null;
			this.localh = 1;
			this.righth = 0;
			this.lefth = 0;
		}

		Node(final int key, final int localh, final int lefth,
				final int righth, final boolean deleted, final int value,
				final Node left, final Node right) {
			this.key = key;
			this.localh = localh;
			this.righth = righth;
			this.lefth = lefth;
			this.deleted = deleted;
			this.left = left;
			this.right = right;
		}

		void setupNode(final int key, final int localh, final int lefth,
				final int righth, final boolean deleted, final Node left,
				final Node right) {
			this.key = key;
			this.localh = localh;
			this.righth = righth;
			this.lefth = lefth;
			this.deleted = deleted;
			this.left = left;
			this.right = right;
		}

		void updateLocalh() {
			this.localh = Math.max(this.lefth + 1, this.righth + 1);
		}

	}

	// state
	private final Node root = new Node(Integer.MAX_VALUE);
	volatile boolean stop = false;
	private final MaintenanceThread mainThd = new MaintenanceThread(this);
	private long rotations = 0, propogations = 0;

	// Constructors
	public TransactionalFriendlyTreeSet() {
		// temporary
		this.startMaintenance();
	}

	@Override
	public void fill(final int range, final long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	@Override
	@Atomic
	public boolean addInt(int x) {
		Node next = this.root, prev = this.root;

		while (next != null) {
			if (next.key > x) {
				prev = next;
				next = next.left;
			} else if (next.key < x) {
				prev = next;
				next = next.right;
			} else {
				if (next.deleted) {
					next.deleted = false;
					return true;
				} else {
					return false;
				}
			}
		}
		Node newNode = new Node(x);
		if (prev.key > x) {
			prev.left = newNode;
		} else if (prev.key < x) {
			prev.right = newNode;
		}
		return true;
	}

	@Override
	@Atomic
	public boolean removeInt(int x) {
		Node next = this.root;

		while (next != null) {
			if (next.key > x) {
				next = next.left;
			} else if (next.key < x) {
				next = next.right;
			} else {
				if (next.deleted) {
					return false;
				} else {
					next.deleted = true;
					return true;
				}
			}
		}
		return false;
	}

	@Override
	@Atomic
	public boolean containsInt(int x) {
		Node next = this.root;

		while (next != null) {
			if (next.key > x) {
				next = next.left;
			} else if (next.key < x) {
				next = next.right;
			} else {
				if (next.deleted) {
					return false;
				} else {
					return true;
				}
			}
		}
		return false;
	}

	@Override
	public Object getInt(int x) {
		if (containsInt(x))
			return x;
		else
			return null;
	}

	@Override
	@Atomic
	public boolean addAll(Collection<Integer> c) {
		boolean result = true;
		for (int x : c)
			result &= this.addInt(x);
		return result;
	}

	@Override
	@Atomic
	public boolean removeAll(Collection<Integer> c) {
		boolean result = true;
		for (int x : c)
			if (!this.removeInt(x))
				result = false;
		return result;
	}

	@Atomic(metainf = "maint")
	private boolean removeNode(Node parent, boolean leftChild) {
		Node node, child;

		if (leftChild) {
			node = parent.left;
		} else {
			node = parent.right;
		}

		if (node == null) {
			return false;
		}

		if (!node.deleted) {
			return false;
		}

		child = node.right;
		if (child == null) {
			child = node.left;
		} else {
			if (node.left != null)
				return false;
		}

		if (leftChild) {
			parent.left = child;
		} else {
			parent.right = child;
		}
		return true;
	}

	@Atomic(metainf = "maint")
	private Node getChild(Node node, boolean leftChild) {
		if (leftChild)
			return node.left;
		return node.right;
	}

	private void recTraverseTree(Node node) {
		Node lChild, rChild;
		if (node == null)
			return;
		if (removeNode(node, true))
			return;
		if (removeNode(node, false))
			return;

		lChild = getChild(node, true);
		recTraverseTree(lChild);
		rChild = getChild(node, false);
		recTraverseTree(rChild);

		if (lChild != null) {
			if (propagate(lChild)) {
				this.performRotation(node, true);
			}
		}
		if (rChild != null) {
			if (propagate(rChild)) {
				this.performRotation(node, false);
			}
		}
	}

	@Atomic(metainf = "maint")
	int rightRotate(Node parent, boolean leftChild, boolean doRotate) {
		Node n, l, lr;

		n = leftChild ? parent.left : parent.right;
		if (n == null)
			return 0;
		l = n.left;
		if (l == null)
			return 0;
		if (l.lefth - l.righth < 0 && !doRotate) {
			// should do a double rotate
			return 2;
		}
		lr = l.right;

		l.right = n;
		n.left = lr;

		n.lefth = l.righth;
		n.updateLocalh();
		l.righth = n.localh;
		l.updateLocalh();

		if (leftChild) {
			parent.left = l;
		} else {
			parent.right = l;
		}

		// need to update balance values
		if (leftChild) {
			parent.lefth = l.localh;
		} else {
			parent.righth = l.localh;
		}
		parent.updateLocalh();
		// System.out.println("right rotate");
		rotations++;
		return 1;
	}

	@Atomic(metainf = "maint")
	int leftRotate(Node parent, boolean leftChild, boolean doRotate) {
		Node n, r, rl;

		n = leftChild ? parent.left : parent.right;
		if (n == null)
			return 0;
		r = n.right;
		if (r == null)
			return 0;
		if (r.lefth - r.righth > 0 && !doRotate) {
			// should do a double rotate
			return 3;
		}
		rl = r.left;

		r.left = n;
		n.right = rl;

		n.righth = r.lefth;
		n.updateLocalh();
		r.lefth = n.localh;
		r.updateLocalh();

		if (leftChild) {
			parent.left = r;
		} else {
			parent.right = r;
		}

		// need to update balance values
		if (leftChild) {
			parent.lefth = r.localh;
		} else {
			parent.righth = r.localh;
		}
		parent.updateLocalh();
		// System.out.println("left rotate");
		rotations++;
		return 1;
	}

	boolean propagate(Node node) {
		Node lchild, rchild;

		lchild = getChild(node, true);
		rchild = getChild(node, false);

		if (lchild == null) {
			node.lefth = 0;
		} else {
			node.lefth = lchild.localh;
		}
		if (rchild == null) {
			node.righth = 0;
		} else {
			node.righth = rchild.localh;
		}

		node.updateLocalh();
		propogations++;

		if (Math.abs(node.righth - node.lefth) >= 2)
			return true;
		return false;
	}

	boolean performRotation(Node parent, boolean goLeft) {
		int ret;
		Node node;

		ret = singleRotation(parent, goLeft, false, false);
		if (ret == 2) {
			// Do a LRR
			node = goLeft ? getChild(parent, true) : getChild(parent, false);
			ret = singleRotation(node, true, true, false);
			if (ret > 0) {
				if (singleRotation(parent, goLeft, false, true) > 0) {
					// System.out.println("LRR");
				}
			}
		} else if (ret == 3) {
			// Do a RLR
			node = goLeft ? getChild(parent, true) : getChild(parent, false);
			ret = singleRotation(node, false, false, true);
			if (ret > 0) {
				if (singleRotation(parent, goLeft, true, false) > 0) {
					// System.out.println("RLR");
				}
			}
		}
		if (ret > 0)
			return true;
		return false;
	}

	int singleRotation(Node parent, boolean goLeft, boolean leftRotation,
			boolean rightRotation) {
		int bal, ret = 0;
		Node node, child;

		node = goLeft ? parent.left : parent.right;
		bal = node.lefth - node.righth;
		if (bal >= 2 || rightRotation) {
			// check reiable and rotate
			child = getChild(node, true);
			if (child != null) {
				if (node.lefth == child.localh) {
					ret = rightRotate(parent, goLeft, rightRotation);
				}
			}
		} else if (bal <= -2 || leftRotation) {
			// check reliable and rotate
			child = getChild(node, false);
			if (child != null) {
				if (node.righth == child.localh) {
					ret = leftRotate(parent, goLeft, leftRotation);
				}
			}
		}
		return ret;
	}

	@Override
	@Atomic
	public int size() {
		return recSize(root.left);
	}

	@Atomic
	public int totalSize() {
		return recTotalSize(root.left);
	}

	public int recTotalSize(Node node) {
		if (node == null)
			return 0;

		int size = 0;
		size += recTotalSize(node.left);
		size += recTotalSize(node.right);

		size++;

		return size;
	}

	public int recSize(Node node) {
		if (node == null)
			return 0;

		int size = 0;
		size += recSize(node.left);
		size += recSize(node.right);

		if (!node.deleted) {
			size++;
		}

		return size;
	}

	@Override
	public void clear() {

	}

	// @Override
	// @Atomic
	// public Object putIfAbsent(int x, int y) {
	// if (!contains(y))
	// add(x);
	// return null;
	// }

	public boolean stopMaintenance() {
		this.stop = true;
		try {
			this.mainThd.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		return true;
	}

	public boolean startMaintenance() {
		this.mainThd.start();
		return true;
	}

	boolean doMaintenance() {
		while (!stop) {
			recTraverseTree(root);
		}
		System.out.println("Rotations: " + rotations);
		System.out.println("Propogations: " + propogations);
		return true;
	}

	@Override
	public long getStructMods() {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int numNodes() {
		return this.totalSize();
	}

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
