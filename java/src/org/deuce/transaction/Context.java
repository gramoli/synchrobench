/*
 * @(#)Context.java   05/01/2008
 *
 * Copyright 2008 GigaSpaces Technologies Inc.
 */

package org.deuce.transaction;

import org.deuce.objectweb.asm.Type;
import org.deuce.transform.Exclude;

/**
 * All the STM implementations should implement this interface.
 * Using the -Dorg.deuce.transaction.contextClass property one can
 * switch between the different implementations. 
 *
 * @author	Guy Korland
 * @since	1.0
 */
@Exclude
public interface Context
{
	final static public Type CONTEXT_TYPE = Type.getType( Context.class);
	final static public String CONTEXT_INTERNAL = Type.getInternalName(Context.class);
	final static public String CONTEXT_DESC = Type.getDescriptor(Context.class);

	/**
	 * Called before the transaction was started
	 * @param atomicBlockId a unique id for atomic block
	 * @param metainf a meta information on the current atomic block.
	 */
	void init(int atomicBlockId, String metainf);

	/**
	 * Called on commit
	 * @return <code>true</code> on success 
	 */
	boolean commit();

	/**
	 * Called on rollback, rollback might be called more than once in a row.
	 * But, can't be called after {@link #commit()} without an {@link #init(int, String)} call in between. 
	 */
	void rollback();

	/* Methods called on Read/Write event */
	void beforeReadAccessStrongIso(Object obj, long field, Object obj2, long fieldObj);
	void beforeReadAccess( Object obj, long field);
	Object onReadAccess( Object obj, Object value, long field);
	boolean onReadAccess( Object obj, boolean value, long field);
	byte onReadAccess( Object obj, byte value, long field);
	char onReadAccess( Object obj, char value, long field);
	short onReadAccess( Object obj, short value, long field);
	int onReadAccess( Object obj, int value, long field);
	long onReadAccess( Object obj, long value, long field);
	float onReadAccess( Object obj, float value, long field);
	double onReadAccess( Object obj, double value, long field);

	void onWriteAccess( Object obj, Object value, long field);
	void onWriteAccess( Object obj, boolean value, long field);
	void onWriteAccess( Object obj, byte value, long field);
	void onWriteAccess( Object obj, char value, long field);
	void onWriteAccess( Object obj, short value, long field);
	void onWriteAccess( Object obj, int value, long field);
	void onWriteAccess( Object obj, long value, long field);
	void onWriteAccess( Object obj, float value, long field);
	void onWriteAccess( Object obj, double value, long field);
	
	void onWriteAccessStrongIso( Object obj, Object value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, boolean value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, byte value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, char value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, short value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, int value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, long value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, float value, long field, long fieldObj);
	void onWriteAccessStrongIso( Object obj, double value, long field, long fieldObj);
}
