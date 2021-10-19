package skiplists.transactional;

import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedList;
import java.util.Map;
import java.util.Random;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

import org.deuce.Atomic;

import contention.abstractions.CompositionalIntSet;
import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;

public class TransactionalFriendlyOptimizedSkipListSet<K,V> implements
		CompositionalIntSet, CompositionalMap<K,V>, MaintenanceAlg {

	static volatile int maxHeight = 1;
	static final int totalHeight = 25;

	/** The thread-private PRNG */
	final private static ThreadLocal<Random> s_random = new ThreadLocal<Random>() {
		@Override
		protected synchronized Random initialValue() {
			return new Random();
		}
	};

	private class MaintenanceThread extends Thread {
		TransactionalFriendlyOptimizedSkipListSet map;

		MaintenanceThread(TransactionalFriendlyOptimizedSkipListSet map) {
			this.map = map;
		}

		public void run() {
			map.doMaintenance();
		}
	}

	static class Index {
		final Node node;
		final Index down;
		volatile Index right;
		Index up;

		/**
		 * Creates index node with given values.
		 */
		Index(Node node, Index down, Index right) {
			this.node = node;
			this.down = down;
			this.right = right;
			this.up = null;
		}
	}

	static final class Node {
		final int key;
		volatile Node next, prev;
		volatile boolean deleted, removed;
		int maxLevel;
		Index up;

		/**
		 * Creates a new regular node.
		 */
		Node(int key) {
			this.key = key;
			this.deleted = false;
			this.removed = false;
			this.maxLevel = 0;
			this.up = null;
		}
	}

	// state
	private final Node begin = new Node(Integer.MIN_VALUE);
	private final Node end = new Node(Integer.MAX_VALUE);
	private final ArrayList<Index> beginList = new ArrayList<Index>();
	private Index startBegin;
	volatile boolean stop = false;
	private final MaintenanceThread mainThd = new MaintenanceThread(this);
	private long heightChanges = 0;

	// Constructors
	public TransactionalFriendlyOptimizedSkipListSet() {

		begin.maxLevel = totalHeight - 1;
		end.maxLevel = totalHeight - 1;
		Index prevBegin = null, prevEnd = null, oldBegin = null, oldEnd = null;
		begin.next = end;
		end.prev = begin;
		for (int i = 1; i < totalHeight; i++) {
			oldBegin = prevBegin;
			oldEnd = prevEnd;
			prevEnd = new Index(end, prevEnd, null);
			prevBegin = new Index(begin, prevBegin, prevEnd);
			if (i == 1) {
				begin.up = prevBegin;
				end.up = prevEnd;
			} else {
				oldBegin.up = prevBegin;
				oldEnd.up = prevEnd;
			}
			beginList.add(prevBegin);
		}
		startBegin = beginList.get(0);
		this.startMaintenance();
	}

	@Override
	public void fill(final int range, final long size) {
		while (this.size() < size) {
			this.addInt(s_random.get().nextInt(range));
		}
	}

	@Atomic
	public boolean addTrans(int x, Node currentNode) {
		Node nextNode;
		while (currentNode.removed) {
			currentNode = currentNode.prev;
		}
		nextNode = currentNode.next;

		for (;;) {
			if (nextNode.key == x) {
				if (!nextNode.deleted) {
					return false;
				} else {
					nextNode.deleted = false;
					return true;
				}
			} else if (nextNode.key > x) {
				Node newNode = new Node(x);
				newNode.next = nextNode;
				newNode.prev = currentNode;
				nextNode.prev = newNode;
				currentNode.next = newNode;
				return true;
			} else {
				currentNode = nextNode;
				nextNode = currentNode.next;
			}
		}
	}

	@Override
	public boolean addInt(int x) {
		Index nextIndex, currentIndex;
		Node currentNode;

		currentIndex = startBegin;
		for (;;) {
			nextIndex = currentIndex.right;
			for (;;) {
				// should only have to go down 1 lvl in the case of null next
				if (nextIndex == null) {
					break;
				}
				if (nextIndex.node.key >= x) {
					break;
				} else {
					currentIndex = nextIndex;
					nextIndex = currentIndex.right;
				}
			}
			if (currentIndex.down == null) {
				break;
			}
			currentIndex = currentIndex.down;
		}

		// System.out.println(traversed);
		currentNode = currentIndex.node;

		return addTrans(x, currentNode);
	}

	@Atomic
	private boolean removeTrans(int x, Node currentNode) {
		Node nextNode;
		while (currentNode.removed) {
			currentNode = currentNode.prev;
		}
		nextNode = currentNode.next;

		for (;;) {
			if (nextNode.key == x) {
				if (!nextNode.deleted) {
					nextNode.deleted = true;
					return true;
				} else {
					return false;
				}
			} else if (nextNode.key > x) {
				return false;
			} else {
				currentNode = nextNode;
				nextNode = currentNode.next;
			}
		}
	}

	@Override
	public boolean removeInt(int x) {
		Index nextIndex, currentIndex;
		Node currentNode;

		currentIndex = startBegin;
		for (;;) {
			nextIndex = currentIndex.right;
			for (;;) {
				// should only have to go down 1 lvl in the case of null next
				if (nextIndex == null) {
					break;
				}
				if (nextIndex.node.key >= x) {
					break;
				} else {
					currentIndex = nextIndex;
					nextIndex = currentIndex.right;
				}
			}
			if (currentIndex.down == null) {
				break;
			}
			currentIndex = currentIndex.down;
		}

		// System.out.println(traversed);
		currentNode = currentIndex.node;
		return removeTrans(x, currentNode);
	}

	@Atomic
	public boolean containsTrans(int x, Node currentNode) {
		Node nextNode;
		while (currentNode.removed) {
			currentNode = currentNode.prev;
		}
		nextNode = currentNode.next;

		for (;;) {
			if (nextNode.key == x) {
				if (!nextNode.deleted) {
					return true;
				} else {
					return false;
				}
			} else if (nextNode.key > x) {
				return false;
			} else {
				currentNode = nextNode;
				nextNode = currentNode.next;
			}
		}
	}

	@Override
	public boolean containsInt(int x) {
		Index nextIndex, currentIndex;
		Node currentNode;

		currentIndex = startBegin;
		for (;;) {
			nextIndex = currentIndex.right;
			for (;;) {
				// should only have to go down 1 lvl in the case of null next
				if (nextIndex == null) {
					break;
				}
				if (nextIndex.node.key >= x) {
					break;
				} else {
					currentIndex = nextIndex;
					nextIndex = currentIndex.right;
				}
			}
			if (currentIndex.down == null) {
				break;
			}
			currentIndex = currentIndex.down;
		}

		// System.out.println(traversed);
		currentNode = currentIndex.node;
		return containsTrans(x, currentNode);
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

	public int totalSize() {
		int count = 0;
		Node current = begin.next;
		ConcurrentHashMap<Integer, Integer> map = new ConcurrentHashMap<Integer, Integer>();
		while (current != end) {
			count++;
			if (map.putIfAbsent((Integer) current.key, (Integer) current.key) != null) {
				System.out.println("Error found 2: " + current.key);
			}
			// System.out.print(current.key);
			current = current.next;
			// System.out.print(current.maxLevel + ", " + current.level + " ");
		}
		System.out.println();
		return count;
	}

	@Override
	@Atomic
	public int size() {
		int count = 0;
		Node current = begin.next;
		while (current != end) {
			if (!current.deleted) {
				count++;
			}
			current = current.next;
			// System.out.print(current.maxLevel + ", " + current.level + " ");
		}
		// System.out.println();
		return count;
	}

	@Override
	public void clear() {
		return;
	}

//	@Override
//	@Atomic
//	public Object putIfAbsent(int x, int y) {
//		if (!contains(y))
//			add(x);
//		return null;
//	}

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

	private void doMaintenance() {
		int count;
		int height;
		while (!stop) {
			count = this.removalTraversal();
			// System.out.println("Count: " + count);
			height = (int) (Math.log(count) / Math.log(2)) + 1;
			// System.out.println("Count, height: " + count + ", " + height);
			if (height > totalHeight) {
				height = totalHeight;
			}

			// this.recCalculateHeight(this.begin, this.end, height - 1, count);
			// this.recCalculateReccomendedHeight(this.begin, this.end, height -
			// 1, count);
			LinkedList<Node> next = null;
			for (int i = 0; i < height; i++) {
				next = doHeight(next, i);
				if (next.size() <= 3)
					break;
			}

			this.raiseNodes();
			setMaxHeight(height);
			// System.out.println("height: " + height);
			setStartBegin(height - 2);
		}
		System.out.println("Height Changes: " + heightChanges);
	}

	// @Atomic
	private void setMaxHeight(int height) {
		maxHeight = height;
	}

	// @Atomic
	private void setStartBegin(int height) {
		startBegin = beginList.get(Math.max(0, height));
	}

	@Atomic(metainf = "maint")
	private Node getNext(Node node) {
		return node.next;
	}

	@Atomic(metainf = "maint")
	private boolean isDeleted(Node node) {
		return node.deleted;
	}

	@Atomic(metainf = "maint")
	public boolean removeMaint(Node prev) {
		Node node = prev.next;
		if (!node.deleted) {
			return false;
		}
		Node next = node.next;
		node.removed = true;
		prev.next = next;
		next.prev = prev;
		return true;
	}

	private int removalTraversal() {
		Node node, next;
		int count = 0, delCount = 0;
		// final double when = 0.05 + 1/Math.pow(10, Math.max(this.totalCount,
		// 1)/Math.max(this.deletedCount, 1));

		count = 0;
		node = begin;
		next = node;
		while (node != end) {
			next = this.getNext(node);
			if (isDeleted(next)) {
				delCount++;
				count++;
			} else {
				count++;
			}
			if (isDeleted(next) && next.maxLevel == 0 /*
													 * && Math.random() > .99
													 * /*&& Math.random() <=
													 * when
													 */) {
				// System.out.println("removing node");
				removeMaint(node);
				// node = node.next;
			} else {
				node = node.next;
			}
		}

		return count;
	}

	public LinkedList<Node> doHeight(LinkedList<Node> list, int height) {
		LinkedList<Node> newList;
		Node prev, curr, next;

		newList = new LinkedList<Node>();

		// if(list != null) {
		// System.out.println("height, list size: " + height + ", " +
		// list.size());
		// }

		if (height == 0) {
			prev = getNext(begin);
			if (prev == end)
				return newList;
			curr = getNext(prev);
			if (curr == end)
				return newList;
			next = getNext(curr);
			if (next == end)
				return newList;
		} else {
			if (list.size() <= 3)
				return newList;
			prev = list.removeFirst();
			curr = list.removeFirst();
			next = list.removeFirst();
		}

		if (prev.maxLevel > height) {
			newList.add(prev);
		}

		while (true) {

			if (curr.maxLevel > height) {
				newList.add(curr);
			} else if (prev.maxLevel <= height && curr.maxLevel <= height
					&& next.maxLevel <= height) {
				curr.maxLevel++;
				// if(curr.maxLevel != height + 1)
				// System.out.println("error in doHeight: " + curr.key);
				newList.add(curr);
			}

			prev = curr;
			curr = next;
			if (height == 0) {
				next = getNext(next);
				// System.out.println(next.maxLevel);
				if (next == end)
					break;
			} else {
				if (list.isEmpty())
					break;
				next = list.removeFirst();
			}
		}
		if (curr.maxLevel > height) {
			newList.add(curr);
		}

		return newList;

	}

	public void raiseNodes() {
		Node node, next;
		Index nodeIndex, nextIndex, tmpIndex;

		for (int i = 1; i < totalHeight; i++) {
			node = begin;
			next = node;
			while (node != end) {
				next = getNext(node);
				// should also check if deleted == false?
				while (next.maxLevel < i) {
					next = getNext(next);
				}
				nextIndex = next.up;
				if (nextIndex == null) {
					nextIndex = new Index(next, null, null);
					next.up = nextIndex;
				}
				for (int j = 2; j < i; j++) {
					nextIndex = nextIndex.up;
				}
				if (nextIndex.up == null) {
					heightChanges++;
					tmpIndex = new Index(next, nextIndex, null);
					nextIndex.up = tmpIndex;
					nextIndex = tmpIndex;
				} else {
					nextIndex = nextIndex.up;
				}

				nodeIndex = node.up;
				for (int j = 2; j <= i; j++) {
					nodeIndex = nodeIndex.up;
				}
				if (nodeIndex.right != nextIndex) {
					nodeIndex.right = nextIndex;
				}

				node = next;
			}
		}

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
