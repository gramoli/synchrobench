/**
 * Contains a field information.
 * 
 * @author Guy Korland
 * @since 1.0
 */
package org.deuce.transform.asm;

import org.deuce.transform.Exclude;

@Exclude
public class Field{
	private final String fieldNameAddress;
	private final String fieldName;
	public String objName = null;

	public Field( String fieldName, String fieldNameAddress) {
		this.fieldName = fieldName;
		this.fieldNameAddress = fieldNameAddress;
	}

	public String getFieldNameAddress() {
		return fieldNameAddress;
	}

	public String getFieldName() {
		return fieldName;
	}
}