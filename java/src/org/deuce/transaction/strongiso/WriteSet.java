package org.deuce.transaction.strongiso;

import java.util.Iterator;

import org.deuce.transaction.strongiso.field.FinalReadFieldAccess;
import org.deuce.transaction.strongiso.field.LocalFieldAccess;
import org.deuce.transaction.strongiso.field.ReadFieldAccess;
import org.deuce.transaction.strongiso.field.WriteFieldAccess;
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
public class WriteSet {

	final private THashSet<WriteFieldAccess> writeSet = new THashSet<WriteFieldAccess>(
			16);
	final LocalFieldAccess read = new LocalFieldAccess();

	public void clear() {
		writeSet.clear();
	}

	public boolean isEmpty() {
		return writeSet.isEmpty();
	}

	public boolean forEach(TObjectProcedure<WriteFieldAccess> procedure) {
		return writeSet.forEach(procedure);
	}

	public Iterator<WriteFieldAccess> getWsIterator() {
		return writeSet.iterator();
	}

	public void put(WriteFieldAccess write) {
		// Add to write set
		if (!writeSet.add(write))
			writeSet.replace(write);
	}

	public WriteFieldAccess contains(FinalReadFieldAccess read) {
		// Check if it is already included in the write set
		return writeSet.get(read);
	}

	public WriteFieldAccess contains(Object ref, long field) {
		// Check if it is already included in the write set
		read.init(ref, field);
		return writeSet.get(read);
	}

	public int size() {
		return writeSet.size();
	}

}
