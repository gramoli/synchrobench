package org.deuce.transform.asm;

import org.deuce.objectweb.asm.MethodVisitor;

/**
 * Holds new fields for transformed classes.
 * @author guy
 * @since 1.1
 */
public interface FieldsHolder {

	void visit(String superName);
	MethodVisitor getStaticMethodVisitor();
	void addField(int fieldAccess, String addressFieldName, String desc, Object value);
	void close();
	String getFieldsHolderName(String owner);
}
