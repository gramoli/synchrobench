package org.deuce.transform.asm;

/**
 * A structure to hold a tuple of class and its bytecode.
 * @author guy
 * @since 1.1
 */
public class ClassByteCode{
	final private String className;
	final private byte[] bytecode;
	
	public ClassByteCode(String className, byte[] bytecode){
		this.className = className;
		this.bytecode = bytecode;
	}

	public String getClassName() {
		return className;
	}

	public byte[] getBytecode() {
		return bytecode;
	}
	
}
