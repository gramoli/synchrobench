package org.deuce.transaction.strongiso.field;

import org.deuce.transaction.tl2.LockTable;
import org.deuce.transform.Exclude;

/**
 * Represents a base class for field write access.  
 * @author Guy Koralnd
 */
@Exclude
public class ReadFieldAccess{
	protected Object reference;
	protected long field;
	private int hash;

	public ReadFieldAccess(){}
	
	public ReadFieldAccess( Object reference, long field){
		init(reference, field);
	}
	
	public void init( Object reference, long field){
		this.reference = reference;
		this.field = field;
		this.hash = (System.identityHashCode( reference) + (int)field);
	}

	@Override
	public boolean equals( Object obj){
		ReadFieldAccess other = (ReadFieldAccess)obj;
		return reference == other.reference && field == other.field;
	}

	@Override
	final public int hashCode(){
		return hash;
	}

	public void clear(){
		reference = null;
	}
}