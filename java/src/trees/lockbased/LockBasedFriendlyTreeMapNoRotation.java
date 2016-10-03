package trees.lockbased;

import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;

import java.util.AbstractMap;
import java.util.Comparator;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.locks.ReentrantLock;

/**
 * The contention-friendly tree implementation of map 
 * as described in:
 *
 * T. Crain, V. Gramoli and M. Ryanla. 
 * A Contention-Friendly Binary Search Tree. 
 * Euro-Par 2013.
 * 
 * @author Tyler Crain
 * 
 * @param <K>
 * @param <V>
 */

public class LockBasedFriendlyTreeMapNoRotation<K, V> extends AbstractMap<K, V> implements
		CompositionalMap<K, V>, MaintenanceAlg {

	static final boolean useFairLocks = false;
	static final boolean allocateOutside = true;
	// we encode directions as characters
	static final char Left = 'L';
	static final char Right = 'R';
	final V DELETED = (V) new Object();

	private class MaintenanceThread extends Thread {
		LockBasedFriendlyTreeMapNoRotation<K, V> map;

		MaintenanceThread(LockBasedFriendlyTreeMapNoRotation<K, V> map) {
			this.map = map;
		}

		public void run() {
			map.doMaintenance();
		}
	}

	private class MaintVariables {
		long propogations = 0, rotations = 0;
	}

	private final MaintVariables vars = new MaintVariables();

	private static class Node<K, V> {
		K key;

		volatile V value;
		volatile Node<K, V> left;
		volatile Node<K, V> right;
		final ReentrantLock lock;
		volatile boolean removed;

		Node(final K key, final V value) {
			this.key = key;
			this.value = value;
			this.removed = false;
			this.lock = new ReentrantLock(useFairLocks);
			this.right = null;
			this.left = null;
		}

		Node(final K key, final int localh, final int lefth, final int righth,
				final V value, final Node<K, V> left, final Node<K, V> right) {
			this.key = key;
			this.value = value;
			this.left = left;
			this.right = right;
			this.lock = new ReentrantLock(useFairLocks);
			this.removed = false;
		}

		void setupNode(final K key, final int localh, final int lefth,
				final int righth, final V value, final Node<K, V> left,
				final Node<K, V> right) {
			this.key = key;
			this.value = value;
			this.left = left;
			this.right = right;
			this.removed = false;
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

	}

	// state
	private final Node<K, V> root = new Node<K, V>(null, null);
	private Comparator<? super K> comparator;
	volatile boolean stop = false;
	private MaintenanceThread mainThd;
	// used in the getSize function
	int size;
	private long structMods = 0;

	// Constructors
	public LockBasedFriendlyTreeMapNoRotation() {
		// temporary
		this.startMaintenance();
	}

	public LockBasedFriendlyTreeMapNoRotation(final Comparator<? super K> comparator) {
		// temporary
		this.startMaintenance();
		this.comparator = comparator;
	}

	// What is this?
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

	@Override
	public boolean containsKey(Object key) {
		if (get(key) == null) {
			return false;
		}
		return true;
	}

	public boolean contains(Object key) {
		if (get(key) == null) {
			return false;
		}
		return true;
	}

	void finishCount(int nodesTraversed) {
		Vars vars = counts.get();
		vars.getCount++;
		vars.nodesTraversed += nodesTraversed;
	}

	@Override
	public V get(final Object key) {
		Node<K, V> next, current;
		next = root;
		final Comparable<? super K> k = comparable(key);
		int rightCmp;

		int nodesTraversed = 0;

		while (true) {
			current = next;
			if (current.key == null) {
				rightCmp = -100;
			} else {
				rightCmp = k.compareTo(current.key);
			}
			if (rightCmp == 0) {
				V value = current.value;
				if (value == DELETED) {
					if (TRAVERSAL_COUNT) {
						finishCount(nodesTraversed);
					}
					return null;
				}
				if (TRAVERSAL_COUNT) {
					finishCount(nodesTraversed);
				}
				return value;
			}
			if (rightCmp <= 0) {
				next = current.left;
			} else {
				next = current.right;
			}
			if (TRAVERSAL_COUNT) {
				nodesTraversed++;
			}
			if (next == null) {
				if (TRAVERSAL_COUNT) {
					finishCount(nodesTraversed);
				}
				return null;
			}
		}
	}

	@Override
	public V remove(final Object key) {
		Node<K, V> next, current;
		next = root;
		final Comparable<? super K> k = comparable(key);
		int rightCmp;
		V value;

		while (true) {
			current = next;
			if (current.key == null) {
				rightCmp = -100;
			} else {
				rightCmp = k.compareTo(current.key);
			}
			if (rightCmp == 0) {
				if (current.value == DELETED) {
					return null;
				}
				current.lock.lock();
				if (!current.removed) {
					break;
				} else {
					current.lock.unlock();
				}
			}
			if (rightCmp <= 0) {
				next = current.left;
			} else {
				next = current.right;
			}
			if (next == null) {
				if (rightCmp != 0) {
					return null;
				}
				// this only happens if node is removed, so you take the
				// opposite path
				// this should never be null
				System.out.println("Going right");
				next = current.right;
			}
		}
		value = current.value;
		if (value == DELETED) {
			current.lock.unlock();
			return null;
		} else {
			current.value = DELETED;
			current.lock.unlock();
			// System.out.println("delete");
			return value;
		}
	}

	@Override
	public V putIfAbsent(K key, V value) {
		int rightCmp;
		Node<K, V> next, current;
		next = root;
		final Comparable<? super K> k = comparable(key);
		Node<K, V> n = null;
		// int traversed = 0;
		V val;

		while (true) {
			current = next;
			// traversed++;
			if (current.key == null) {
				rightCmp = -100;
			} else {
				rightCmp = k.compareTo(current.key);
			}
			if (rightCmp == 0) {
				val = current.value;
				if (val != DELETED) {
					// System.out.println(traversed);
					return val;
				}
				current.lock.lock();
				if (!current.removed) {
					break;
				} else {
					current.lock.unlock();
				}
			}
			if (rightCmp <= 0) {
				next = current.left;
			} else {
				next = current.right;
			}
			if (next == null) {
				if (n == null && allocateOutside) {
					n = new Node<K, V>(key, value);
				}
				current.lock.lock();
				if (!current.removed) {
					if (rightCmp <= 0) {
						next = current.left;
					} else {
						next = current.right;
					}
					if (next == null) {
						break;
					} else {
						current.lock.unlock();
					}
				} else {
					current.lock.unlock();
					// maybe have to check if the other one is still null before
					// going the opposite way?
					// YES!! We do this!
					if (rightCmp <= 0) {
						next = current.left;
					} else {
						next = current.right;
					}
					if (next == null) {
						if (rightCmp > 0) {
							next = current.left;
						} else {
							next = current.right;
						}
					}
				}
			}
		}
		val = current.value;
		if (rightCmp == 0) {
			if (val == DELETED) {
				current.value = value;
				current.lock.unlock();
				// System.out.println("insert");
				// System.out.println(traversed);
				return null;
			} else {
				current.lock.unlock();
				return val;
			}
		} else {
			if (!allocateOutside) {
				n = new Node<K, V>(key, value);
			}
			if (rightCmp <= 0) {
				current.left = n;
			} else {
				current.right = n;
			}
			current.lock.unlock();
			// System.out.println(traversed);
			// System.out.println("insert");
			return null;
		}
	}

	@Override
	public Set<Entry<K, V>> entrySet() {
		// TODO Auto-generated method stub
		return null;
	}

	// maintenance
	boolean removeNode(Node<K, V> parent, char direction) {
		Node<K, V> n, child;
		// can get before locks because only maintenance removes nodes
		if (parent.removed)
			return false;
		n = direction == Left ? parent.left : parent.right;
		if (n == null)
			return false;
		// get the locks
		n.lock.lock();
		parent.lock.lock();
		if (n.value != DELETED) {
			n.lock.unlock();
			parent.lock.unlock();
			return false;
		}
		if ((child = n.left) != null) {
			if (n.right != null) {
				n.lock.unlock();
				parent.lock.unlock();
				return false;
			}
		} else {
			child = n.right;
		}
		if (direction == Left) {
			parent.left = child;
		} else {
			parent.right = child;
		}
		n.left = parent;
		n.right = parent;
		n.removed = true;
		n.lock.unlock();
		parent.lock.unlock();
		// System.out.println("removed a node");
		// need to update balance values here
		return true;
	}

	boolean recursivePropagate(Node<K, V> parent, Node<K, V> node,
			char direction) {
		Node<K, V> left, right;

		if (node == null)
			return true;
		left = node.left;
		right = node.right;

		if (!node.removed && node.value == DELETED
				&& (left == null || right == null) && node != this.root) {
			if (removeNode(parent, direction)) {
				return true;
			}
		}

		if (stop) {
			return true;
		}

		if (!node.removed) {
			if (left != null) {
				recursivePropagate(node, left, Left);
			}
			if (right != null) {
				recursivePropagate(node, right, Right);
			}
		}

		return true;
	}

	public boolean stopMaintenance() {
		this.stop = true;
		try {
			this.mainThd.join();
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return true;
	}

	public boolean startMaintenance() {
		this.stop = false;

		mainThd = new MaintenanceThread(this);

		mainThd.start();

		return true;
	}

	boolean doMaintenance() {
		while (!stop) {
			recursivePropagate(this.root, this.root.left, Left);
		}
		if (STRUCT_MODS)
			this.structMods += counts.get().structMods;
		System.out.println("Propogations: " + vars.propogations);
		System.out.println("Rotations: " + vars.rotations);
		System.out.println("Max depth: " + recursiveDepth(root.left));
                System.out.println("Average depth: " + averageDepth());
                System.out.println("Total depth: " + totalDepth(root.left, 0));
                System.out.println("Hash: " + hash());
		return true;
	}

	// not thread safe
	public int getSize() {
		this.size = 0;
		recursiveGetSize(root.left);
		return size;
	}

	void recursiveGetSize(Node<K, V> node) {
		if (node == null)
			return;
		if (node.removed) {
			// System.out.println("Shouldn't find removed nodes in the get size function");
		}
		if (node.value != DELETED) {
			this.size++;
		}
		recursiveGetSize(node.left);
		recursiveGetSize(node.right);
	}

	public int numNodes() {
		this.size = 0;
		ConcurrentHashMap<Integer, Node<K, V>> map = new ConcurrentHashMap<Integer, Node<K, V>>();
		recursiveNumNodes(root.left, map);
		return size;
	}

	void recursiveNumNodes(Node<K, V> node,
			ConcurrentHashMap<Integer, Node<K, V>> map) {
		if (node == null)
			return;
		if (node.removed) {
			// System.out.println("Shouldn't find removed nodes in the get size function");
		}
		Node<K, V> n = map.putIfAbsent((Integer) node.key, node);
		if (n != null) {
			System.out.println("Error: " + node.key);
		}
		this.size++;
		recursiveNumNodes(node.left, map);
		recursiveNumNodes(node.right, map);
	}

	public int getBalance() {
		int lefth = 0, righth = 0;
		if (root.left == null)
			return 0;
		lefth = recursiveDepth(root.left.left);
		righth = recursiveDepth(root.left.right);
		return lefth - righth;
	}

	int recursiveDepth(Node<K, V> node) {
		if (node == null) {
			return 0;
		}
		int lefth, righth;
		lefth = recursiveDepth(node.left);
		righth = recursiveDepth(node.right);
		return Math.max(lefth, righth) + 1;
	}

    int totalDepth(Node<K, V> node, int d) {
        if (node == null) {
            return 0;
        }
        return (node.value != DELETED ? d : 0) + totalDepth(node.left, d + 1) + totalDepth(node.right, d + 1);
    }

    int averageDepth() {
        return totalDepth(root.left, 0) / size();
    }

        int hash(Node<K, V> node, int power) {
            if (node == null) {
                return 0;
            }
            return node.value.hashCode() * power + hash(node.left, power * 239) + hash(node.right, power * 533);
        }

        int hash() {
            return hash(root.left, 1);
        }

	@Override
	public void clear() {
		this.stopMaintenance();
		this.resetTree();
		this.startMaintenance();

		return;
	}

	private void resetTree() {
		this.structMods = 0;
		this.vars.propogations = 0;
		this.vars.rotations = 0;
		root.left = null;
	}

	@Override
	public int size() {
		return this.getSize();
	}

	public long getStructMods() {
		return structMods;
	}

}
