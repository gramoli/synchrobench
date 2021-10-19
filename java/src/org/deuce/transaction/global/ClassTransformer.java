package org.deuce.transaction.global;

import org.deuce.objectweb.asm.MethodVisitor;
import org.deuce.transform.asm.ByteCodeVisitor;

public class ClassTransformer extends ByteCodeVisitor{

	public ClassTransformer( String className) {
		super( className);
	}
	
	@Override
	public MethodVisitor visitMethod( int access, String name, String desc,
			String signature, String[] exceptions) {
		MethodVisitor mv = super.visitMethod(access, name, desc, signature, exceptions);
		return new MethodTransformer( mv, access, name, desc, signature, exceptions, this);
	}
	
	public MethodVisitor createMethod( int access, String name, String desc,
			String signature, String[] exceptions) {
		return super.visitMethod(access, name, desc, signature, exceptions);
	}
}
