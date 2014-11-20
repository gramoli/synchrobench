package org.deuce.transaction.util;

/**
 * Dynamic boolean array.
 *
 * @author	Guy Korland
 * @since	1.0
 */
public class BooleanArrayList{

    private static final int DEFAULT_CAPACITY = 100;
    private boolean[] array = new boolean[DEFAULT_CAPACITY];

    public void insert(int offset, boolean value) {
        ensureCapacity(offset + 1);
        array[offset] = value;
    }

    public boolean get(int offset) {
    	ensureCapacity(offset + 1);
        return array[offset];
    }
    
    private void ensureCapacity(int capacity) {
        if (capacity > array.length) {
            int newCap = Math.max(array.length << 1, capacity);
            boolean[] tmp = new boolean[newCap];
            System.arraycopy(array, 0, tmp, 0, array.length);
            array = tmp;
        }
    }

} 
