package contention.benchmark;

import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;
import java.util.Vector;
import java.util.concurrent.ConcurrentHashMap;

import contention.abstractions.CompositionalMap;

/**
 * The loop executed by each thread of the map 
 * benchmark.
 * 
 * @author Vincent Gramoli
 * 
 */
public class ThreadLoop implements Runnable {

	/** The instance of the running benchmark */
	public CompositionalMap<Integer, Integer> bench;
	/** The stop flag, indicating whether the loop is over */
	protected volatile boolean stop = false;
	/** The pool of methods that can run */
	protected Method[] methods;
	/** The number of the current thread */
	protected final short myThreadNum;

	/** The counters of the thread successful operations */
	public long numAdd = 0;
	public long numRemove = 0;
	public long numAddAll = 0;
	public long numRemoveAll = 0;
	public long numSize = 0;
	public long numContains = 0;
	/** The counter of the false-returning operations */
	public long failures = 0;
	/** The counter of the thread operations */
	public long total = 0;
	/** The counter of aborts */
	public long aborts = 0;
	/** The random number */
	Random rand = new Random();

	public long getCount;
	public long nodesTraversed;
	public long structMods;

	/**
	 * The distribution of methods as an array of percentiles
	 * 
	 * 0%        cdf[0]        cdf[2]                     100%
	 * |--writeAll--|--writeSome--|--readAll--|--readSome--|
	 * |-----------write----------|--readAll--|--readSome--| cdf[1]
	 */
	int[] cdf = new int[3];

	public ThreadLoop(short myThreadNum,
			CompositionalMap<Integer, Integer> bench, Method[] methods) {
		this.myThreadNum = myThreadNum;
		this.bench = bench;
		this.methods = methods;
		/* initialize the method boundaries */
		assert (Parameters.numWrites >= Parameters.numWriteAlls);
		cdf[0] = 10 * Parameters.numWriteAlls;
		cdf[1] = 10 * Parameters.numWrites;
		cdf[2] = cdf[1] + 10 * Parameters.numSnapshots;
	}

	public void stopThread() {
		stop = true;
	}

	public void printDataStructure() {
		System.out.println(bench.toString());
	}

	public void run() {

		while (!stop) {
			Integer newInt = rand.nextInt(Parameters.range);
			Integer a, b;
			int coin = rand.nextInt(1000);
			if (coin < cdf[0]) { // 1. should we run a writeAll operation?

				// init a collection
				Map<Integer, Integer> hm = new HashMap<Integer, Integer>(newInt, newInt);
				hm.put(newInt / 2, newInt / 2); // accepts duplicate

				try {
					if (bench.keySet().removeAll(hm.keySet()))
						numRemoveAll++; 
					else failures++; 
				} catch (Exception e) {
					System.err.println("Unsupported writeAll operations! Leave the default value of the numWriteAlls parameter (0).");
				}
				 

			} else if (coin < cdf[1]) { // 2. should we run a writeSome
										// operation?

				if (2 * (coin - cdf[0]) < cdf[1] - cdf[0]) { // add
					if ((a = bench.putIfAbsent((int) newInt, (int) newInt)) == null) {
						numAdd++;
					} else {
						failures++;
					}
				} else { // remove
					if ((a = bench.remove((int) newInt)) != null) {
						numRemove++;
					} else
						failures++;
				}

			} else if (coin < cdf[2]) { // 3. should we run a readAll operation?

				bench.size();
				numSize++;

			} else { // 4. then we should run a readSome operation

				if (bench.get((int) newInt) != null)
					numContains++;
				else
					failures++;
			}
			total++;

			assert total == failures + numContains + numSize + numRemove
					+ numAdd + numRemoveAll + numAddAll;
		}
		// System.out.println(numAdd + " " + numRemove + " " + failures);
		this.getCount = CompositionalMap.counts.get().getCount;
		this.nodesTraversed = CompositionalMap.counts.get().nodesTraversed;
		this.structMods = CompositionalMap.counts.get().structMods;
		System.out.println("Thread #" + myThreadNum + " finished.");
	}
}
