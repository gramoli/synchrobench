package contention.benchmark;

import contention.abstractions.CompositionalMap;

import java.lang.reflect.Method;
import java.lang.reflect.Parameter;
import java.util.HashMap;
import java.util.Map;
import java.util.Random;

/**
 * The loop executed by each thread of the map 
 * benchmark.
 * 
 * @author Vincent Gramoli
 * 
 */
public class ThreadSkewedLoop extends ThreadLoop implements Runnable {
	/** The integers to operate with **/
	protected int[] operated;

	/**
	 * The distribution of methods as an array of percentiles
	 *
	 * 0%        cdf[0]        cdf[2]                     100%
	 * |--writeAll--|--writeSome--|--readAll--|--readSome--|
	 * |-----------write----------|--readAll--|--readSome--| cdf[1]
	 */
	int[] cdf = new int[3];

	public ThreadSkewedLoop(short myThreadNum,
							CompositionalMap<Integer, Integer> bench, Method[] methods) {
		super(myThreadNum, bench, methods);

		operated = new int[Parameters.skewed * Parameters.numThreads];
		for (int i = 0; i < operated.length; i++) {
			operated[i] = -i * Parameters.numThreads - myThreadNum;
		}
	}

	public void run() {
		int id = 0;
		while (!stop) {
			Integer a, b;
			int coin = rand.nextInt(1000);
			if (coin < cdf[1]) {
				if (id < operated.length) {
					bench.putIfAbsent(operated[id], operated[id]);
					numAdd++;
				} else {
					bench.remove(operated[id - operated.length]);
					numRemove++;
				}

				id = (id + 1) % operated.length;
			} else {

				if (bench.get(operated[rand.nextInt(operated.length)]) != null)
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
