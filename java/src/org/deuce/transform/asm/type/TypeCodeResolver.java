package org.deuce.transform.asm.type;

import org.deuce.objectweb.asm.Type;

public abstract class TypeCodeResolver {
	private final Type type;

	public TypeCodeResolver( Type type){
		this.type = type;
	}

	abstract public int loadCode();
	abstract public int storeCode();
	abstract public int returnCode();
	abstract public int nullValueCode();
	
	/** Returns this type size in the Locals table */
	public int localSize(){
		return 1; //  no extend, 32 bit 
	}
	
	@Override
	public String toString(){
		return type.toString();
	}
}
