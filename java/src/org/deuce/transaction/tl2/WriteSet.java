package org.deuce.transaction.tl2;


import org.deuce.transaction.tl2.field.ReadFieldAccess;
import org.deuce.transaction.tl2.field.WriteFieldAccess;
import org.deuce.transform.Exclude;
import org.deuce.trove.THashSet;
import org.deuce.trove.TObjectProcedure;

/**
 * Represents the transaction write set.
 *  
 * @author Guy Korland
 * @since 0.7
 */
@Exclude
public class WriteSet{
	
	final private THashSet<WriteFieldAccess> writeSet = new THashSet<WriteFieldAccess>( 16);
	
	public void clear() {
		writeSet.clear();
	}

	public boolean isEmpty() {
		return writeSet.isEmpty();
	}

	public boolean forEach(TObjectProcedure<WriteFieldAccess> procedure){
		return writeSet.forEach(procedure);
	}
	
	public void put(WriteFieldAccess write) {
		// Add to write set
		if(!writeSet.add( write))
			writeSet.replace(write);
	}
	
	public WriteFieldAccess contains(ReadFieldAccess read) {
		// Check if it is already included in the write set
		return writeSet.get( read);
	}
	
	public int size() {
		return writeSet.size();
	}
	
}
