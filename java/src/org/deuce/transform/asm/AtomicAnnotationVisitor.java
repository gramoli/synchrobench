package org.deuce.transform.asm;

import org.deuce.objectweb.asm.AnnotationVisitor;
import org.deuce.transform.asm.method.AtomicMethod;

public class AtomicAnnotationVisitor implements AnnotationVisitor {

	private final AtomicMethod method;
	private final AnnotationVisitor annotation;

	public AtomicAnnotationVisitor(AtomicMethod method, AnnotationVisitor annotation) {
		this.method = method;
		this.annotation = annotation;
	}

	public void visit(String name, Object value) {
		method.setRetries((Integer)value);
		annotation.visit(name, value);
	}

	public AnnotationVisitor visitAnnotation(String name, String desc) {
		return annotation.visitAnnotation(name, desc);
	}

	public AnnotationVisitor visitArray(String name) {
		return annotation.visitArray(name);
	}

	public void visitEnd() {
		annotation.visitEnd();
	}

	public void visitEnum(String name, String desc, String value) {
		annotation.visitEnum(name, desc, value);
	}

}
