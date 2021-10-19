package org.deuce.transaction.strongiso;

import org.deuce.transform.Exclude;

/**
 * Implements Bloom filter map
 * 
 * @author Guy Korland
 * @since 1.0
 *
 */
@Exclude
public class BloomFilter {
	
	final private static short BLOOM_FILTER_SIZE = 1 << 10; // the array length
	final static private byte[] EMPTY_FILTER = new byte[BLOOM_FILTER_SIZE];
	
	private byte[] bloomFilter = new byte[BLOOM_FILTER_SIZE];

	public boolean contains( int hash) {

		int bit1 = hash & 0x3FF; // first 10 bits of the first part of the int
		int bit2 = (hash >>> Short.SIZE) & 0x3FF; // first 10 bits of the second part of the int

		return ((bloomFilter[ bit1/Byte.SIZE] & (1 << bit1%Byte.SIZE)) != 0) &&
		((bloomFilter[ bit2/Byte.SIZE] & (1 << bit2%Byte.SIZE)) != 0);
	}

	public void add( int hash) {

		int bit1 = hash & 0x3FF; // first 10 bits of the first part of the int
		int bit2 = (hash >>> Short.SIZE) & 0x3FF; // first 10 bits of the second part of the int

		bloomFilter[ bit1/Byte.SIZE] |= (1 << bit1%Byte.SIZE);
		bloomFilter[ bit2/Byte.SIZE] |= (1 << bit2%Byte.SIZE);
	}

	public void clear() {
		 // TODO check if can clear faster or even call new
		System.arraycopy(EMPTY_FILTER, 0, bloomFilter, 0, BLOOM_FILTER_SIZE);
	}
}
