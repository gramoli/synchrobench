package org.deuce.transaction.strongiso.field;

import org.deuce.transform.Exclude;

/**
 * Represents a base class for field write access.
 * 
 * @author Guy Koralnd
 */
@Exclude
public class ArrayReadFieldAccess extends ReadFieldAccess {
	ReadFieldAccess[] array;
}