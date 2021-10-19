package hashtables.lockfree;

import java.util.Collection;
import java.util.Map;
import java.util.Set;

import sun.misc.Unsafe;
import contention.abstractions.CompositionalMap;
import contention.abstractions.MaintenanceAlg;
import hashtables.lockfree.cliffutils.UtilUnsafe;

/**
 * This code corresponds to the contention-friendly hash map
 * as described in:
 * 
 * T. Crain, V. Gramoli and M. Raynal. A Contention-Friendly 
 * Methodology for Search Structures. TR #hal-00668010, INRIA, 
 * 2012. 
 * 
 * @author Tyler Crain
 */
public class NonBlockingFriendlyHashMap<K, V> implements
		CompositionalMap<K, V>, MaintenanceAlg {

	static final int DEFAULT_INITIAL_CAPACITY = 16;

	static final float DEFAULT_LOAD_FACTOR = 0.75f;

	private long structMods = 0;

	volatile boolean stop = false;
	private MaintenanceThread mainThd;

	private class MaintenanceThread<K, V> extends Thread {
		NonBlockingFriendlyHashMap<K, V> map;

		MaintenanceThread(NonBlockingFriendlyHashMap<K, V> map) {
			this.map = map;
		}

		public void run() {
			map.doMaintenance();
		}
	}

	private static long rawIndex(final Object[] ary, final int idx) {
		assert idx >= 0 && idx < ary.length;
		return _Obase + idx * _Oscale;
	}

	public void doMaintenance() {
		int size;
		while (!stop) {
			size = size();
			if (size > threshold) {
				rehash();
			}
		}
	}

	public boolean startMaintenance() {

		this.stop = false;
		mainThd = new MaintenanceThread<K, V>(this);
		mainThd.start();

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

	// --- Bits to allow Unsafe access to arrays
	private static Unsafe _unsafe = UtilUnsafe.getUnsafe();
	private static final int _Obase = _unsafe.arrayBaseOffset(Object[].class);
	private static final int _Oscale = _unsafe.arrayIndexScale(Object[].class);

	private static final boolean CAS_val(Object[] kvs, int idx, Object old,
			Object val) {
		// Do I need to shift the index bit, like done in the lock free table???
		return _unsafe
				.compareAndSwapObject(kvs, rawIndex(kvs, (idx)), old, val);
	}

	int threshold;
	float loadFactor;
	int cap;

	class Table {
		final transient HashEntry<K, V>[] table;
		final HashEntry<K, V> dummy;

		public Table(int size) {
			this.table = new HashEntry[size];
			this.dummy = new HashEntry<K, V>(null, 0, null, null);

			// for(int i = 0; i < this.table.length; i++) {
			// table[i] = null;
			// }
		}
	}

	volatile transient Table table1, table2;

	static final class HashEntry<K, V> {
		final K key;
		final int hash;
		volatile V value;
		final HashEntry<K, V> next;

		HashEntry(K key, int hash, HashEntry<K, V> next, V value) {
			this.key = key;
			this.hash = hash;
			this.next = next;
			this.value = value;
		}

		@SuppressWarnings("unchecked")
		static final <K, V> HashEntry<K, V>[] newArray(int i) {
			return new HashEntry[i];
		}
	}

	public NonBlockingFriendlyHashMap(int initialCapacity, float loadFactor) {
		if (!(loadFactor > 0) || initialCapacity < 0)
			throw new IllegalArgumentException();

		cap = 1;
		while (cap < initialCapacity)
			cap <<= 1;
		this.loadFactor = loadFactor;

		this.table1 = new Table(cap);
		this.threshold = (int) (table1.table.length * loadFactor);
		this.startMaintenance();
	}

	public NonBlockingFriendlyHashMap(int initialCapacity) {
		this(initialCapacity, DEFAULT_LOAD_FACTOR);
	}

	public NonBlockingFriendlyHashMap() {
		this(DEFAULT_INITIAL_CAPACITY, DEFAULT_LOAD_FACTOR);
	}

	Table getTable(Table oldTable) {
		Table tab2 = table2;
		if (oldTable == table1)
			return tab2;
		return table1;
	}

	HashEntry<K, V> getFirst(int hash) {
		Table table = table1;
		HashEntry<K, V>[] tab = table.table;
		HashEntry<K, V> node = tab[hash & (tab.length - 1)];
		while (node == table.dummy) {
			table = getTable(table);
			tab = table.table;
			node = tab[hash & (tab.length - 1)];
		}
		return node;
	}

	private static int hash(int h) {
		// Spread bits to regularize both segment and index locations,
		// using variant of single-word Wang/Jenkins hash.
		h += (h << 15) ^ 0xffffcd7d;
		h ^= (h >>> 10);
		h += (h << 3);
		h ^= (h >>> 6);
		h += (h << 2) + (h << 14);
		return h ^ (h >>> 16);
	}

	@Override
	public boolean containsKey(Object key) {
		if (this.get(key) == null) {
			return false;
		}
		return true;
	}

	@Override
	public V remove(final Object key) {
		HashEntry<K, V>[] tab;
		// int hash = hash(key.hashCode());
		int hash = key.hashCode();
		Table table;
		V oldValue;
		int index;
		HashEntry<K, V> first, e;

		while (true) {
			table = table1;
			tab = table.table;
			index = hash & (tab.length - 1);
			first = tab[index];
			while (first == table.dummy) {
				table = getTable(table);
				tab = table.table;
				index = hash & (tab.length - 1);
				first = tab[index];
			}

			e = first;
			while (e != null && (e.hash != hash || !key.equals(e.key)))
				e = e.next;

			oldValue = null;
			if (e != null) {
				oldValue = e.value;
				// All entries following removed node can stay
				// in list, but all preceding ones need to be
				// cloned.
				HashEntry<K, V> newFirst = e.next;
				for (HashEntry<K, V> p = first; p != e; p = p.next)
					newFirst = new HashEntry<K, V>(p.key, p.hash, newFirst,
							p.value);
				if (CAS_val(tab, index, first, newFirst)) {
					break;
				}
			} else {
				break;
			}
		}
		return oldValue;
	}

	private void rehash() {
		int oldCapacity = table1.table.length;
		table2 = new Table(oldCapacity << 1);
		threshold = (int) (table2.table.length * loadFactor);
		int sizeMask = table2.table.length - 1;
		for (int i = 0; i < oldCapacity; i++) {
			rehashLevel(i, sizeMask);
		}
		// System.out.println("done rehash");
		table1 = table2;
		if (STRUCT_MODS)
			this.structMods += 1;
	}

	private void rehashLevel(int i, int sizeMask) {
		HashEntry<K, V> e, dummy = table1.dummy;
		HashEntry<K, V>[] oldTable = table1.table;
		HashEntry<K, V>[] newTable = table2.table;
		while (true) {

			newTable[i] = null;
			newTable[i + table1.table.length] = null;

			e = oldTable[i];

			if (e != null) {
				HashEntry<K, V> next = e.next;
				int idx = e.hash & sizeMask;

				// Single node on list
				if (next == null)
					newTable[idx] = e;

				else {
					// Reuse trailing consecutive sequence at same slot
					HashEntry<K, V> lastRun = e;
					int lastIdx = idx;
					for (HashEntry<K, V> last = next; last != null; last = last.next) {
						int k = last.hash & sizeMask;
						if (k != lastIdx) {
							lastIdx = k;
							lastRun = last;
						}
					}
					newTable[lastIdx] = lastRun;

					// Clone all remaining nodes
					for (HashEntry<K, V> p = e; p != lastRun; p = p.next) {
						int k = p.hash & sizeMask;
						HashEntry<K, V> n = newTable[k];
						newTable[k] = new HashEntry<K, V>(p.key, p.hash, n,
								p.value);
					}
				}
			}
			if (CAS_val(oldTable, i, e, dummy)) {
				break;
			}
		}
	}

	@Override
	public V putIfAbsent(K key, V value) {

		HashEntry<K, V>[] tab;
		// int hash = hash(key.hashCode());
		int hash = key.hashCode();
		Table table;
		V oldValue;
		int index;
		HashEntry<K, V> first, e;

		while (true) {
			table = table1;
			tab = table.table;
			index = hash & (tab.length - 1);
			first = tab[index];
			while (first == table.dummy) {
				table = getTable(table);
				tab = table.table;
				index = hash & (tab.length - 1);
				first = tab[index];
			}

			e = first;
			while (e != null && (e.hash != hash || !key.equals(e.key)))
				e = e.next;

			if (e != null) {
				oldValue = e.value;
				break;
			} else {
				oldValue = null;
				HashEntry<K, V> newEntry = new HashEntry<K, V>(key, hash,
						first, value);
				if (CAS_val(tab, index, first, newEntry)) {
					break;
				}
			}
		}
		return oldValue;
	}

	@Override
	public void clear() {
		this.stopMaintenance();
		this.structMods = 0;
		HashEntry<K, V>[] tab = table1.table;
		for (int i = 0; i < tab.length; i++) {
			tab[i] = null;
		}
		this.startMaintenance();
	}

	@Override
	public int size() {
		HashEntry<K, V>[] tab = table1.table;
		HashEntry<K, V> next;
		int count = 0;

		for (int i = 0; i < tab.length; i++) {
			next = tab[i];
			while (next != null) {
				count++;
				next = next.next;
			}
		}
		return count;
	}

	@Override
	public V get(final Object key) {
		// int hash = hash(key.hashCode());
		int hash = key.hashCode();
		HashEntry<K, V> e = getFirst(hash);

		while (e != null) {
			if (e.hash == hash && key.equals(e.key)) {
				V v = e.value;
				if (v != null)
					return v;
				// is the following really necessary?
				while (v == null) {
					v = e.value;
				}
				return v;
			}
			e = e.next;
		}

		return null;
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

	@Override
	public long getStructMods() {
		return structMods;
	}

	@Override
	public int numNodes() {
		// TODO Auto-generated method stub
		return 0;
	}

}
