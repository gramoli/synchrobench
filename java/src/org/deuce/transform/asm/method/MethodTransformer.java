package org.deuce.transform.asm.method;

import java.util.HashMap;

import org.deuce.Unsafe;
import org.deuce.objectweb.asm.AnnotationVisitor;
import org.deuce.objectweb.asm.Attribute;
import org.deuce.objectweb.asm.Label;
import org.deuce.objectweb.asm.MethodVisitor;
import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;
import org.deuce.objectweb.asm.commons.AnalyzerAdapter;
import org.deuce.objectweb.asm.commons.Method;
import org.deuce.transform.asm.FieldsHolder;

public class MethodTransformer implements MethodVisitor{

	final static private String UNSAFE_DESCRIPTOR = Type.getDescriptor(Unsafe.class);
	
	private MethodVisitor originalMethod;

	final private MethodVisitor originalCopyMethod;
	
	private MethodVisitor copyMethod;
	final private String className;
	final private String methodName;
	final private String descriptor; // original descriptor
	final private HashMap<Label, Label> labelMap = new HashMap<Label, Label>();
	final private boolean isStatic;
	final private Method newMethod;

	public MethodTransformer(MethodVisitor originalMethod, MethodVisitor copyMethod, 
			String className, int access, String methodName, String descriptor, Method newMethod,
			FieldsHolder fieldsHolder) {
		
		this.originalMethod = originalMethod;
		this.newMethod = newMethod;
		this.isStatic = (access & Opcodes.ACC_STATIC) != 0;
		this.originalCopyMethod = copyMethod;
		
		// The AnalyzerAdapter delegates the call to the DuplicateMethod, while the DuplicateMethod uses
		// the analyzer for stack state in the original method.
		DuplicateMethod duplicateMethod = new DuplicateMethod( copyMethod, isStatic, newMethod, fieldsHolder);
		AnalyzerAdapter analyzerAdapter = new AnalyzerAdapter( className, access, methodName, descriptor, duplicateMethod);
		duplicateMethod.setAnalyzer( analyzerAdapter);
		
		this.copyMethod = analyzerAdapter;
		this.className = className;
		this.methodName = methodName;
		this.descriptor = descriptor;
	}

	public void visitCode() {
		originalMethod.visitCode();
		copyMethod.visitCode();
	}

	public AnnotationVisitor visitAnnotation(String desc, boolean visible) {
		
		// FIXME we might see other annotations before and we need to put it on the new AtomicMethod
		// need to create an atomic method from the original method
		if( AtomicMethod.ATOMIC_DESCRIPTOR.equals(desc) && !(originalMethod instanceof AtomicMethod))
			originalMethod = new AtomicMethod( originalMethod, className, methodName,
					descriptor, newMethod, isStatic);

		if( UNSAFE_DESCRIPTOR.equals(desc)) // if marked as Unsafe no just duplicate the method as is.
			copyMethod = originalCopyMethod;
		
		if( !desc.contains("org/junit")) // TODO find another way
			return new MethodAnnotationVisitor( originalMethod.visitAnnotation(desc, visible),
					copyMethod.visitAnnotation(desc, visible));
		else	
			return originalMethod.visitAnnotation(desc, visible);
	}

	public AnnotationVisitor visitAnnotationDefault() {
		return new MethodAnnotationVisitor( originalMethod.visitAnnotationDefault(), 
				copyMethod.visitAnnotationDefault());
	}

	public void visitAttribute(Attribute attr) {
		originalMethod.visitAttribute(attr);
		copyMethod.visitAttribute(attr);
	}

	public void visitEnd() {
		originalMethod.visitEnd();
		copyMethod.visitEnd();
	}

	public void visitFieldInsn(int opcode, String owner, String name, String desc) {
		originalMethod.visitFieldInsn(opcode, owner, name, desc);
		copyMethod.visitFieldInsn(opcode, owner, name, desc);
	}

	public void visitFrame(int type, int local, Object[] local2, int stack, Object[] stack2) {
		originalMethod.visitFrame(type, local, local2, stack, stack2);
		copyMethod.visitFrame(type, local, local2, stack, stack2);
	}

	public void visitIincInsn(int var, int increment) {
		originalMethod.visitIincInsn(var, increment);
		copyMethod.visitIincInsn(var, increment);
	}

	public void visitInsn(int opcode) {
		originalMethod.visitInsn(opcode);
		copyMethod.visitInsn(opcode);
	}

	public void visitIntInsn(int opcode, int operand) {
		originalMethod.visitIntInsn(opcode, operand);
		copyMethod.visitIntInsn(opcode, operand);
	}

	public void visitJumpInsn(int opcode, Label label) {
		originalMethod.visitJumpInsn(opcode, label);
		copyMethod.visitJumpInsn(opcode, getLabel(label));
	}

	public void visitLabel(Label label) {
		originalMethod.visitLabel(label);
		copyMethod.visitLabel(getLabel(label));
	}

	public void visitLdcInsn(Object cst) {
		originalMethod.visitLdcInsn(cst);
		copyMethod.visitLdcInsn(cst);
	}


	public void visitLineNumber(int line, Label start) {
		originalMethod.visitLineNumber(line, start);
		copyMethod.visitLineNumber(line, getLabel(start));
	}


	public void visitLocalVariable(String name, String desc, String signature, Label start,
			Label end, int index) {
		originalMethod.visitLocalVariable(name, desc, signature, start, end, index);
		copyMethod.visitLocalVariable(name, desc, signature, getLabel(start), getLabel(end), index);
	}


	public void visitLookupSwitchInsn(Label dflt, int[] keys, Label[] labels) {
		originalMethod.visitLookupSwitchInsn(dflt, keys, labels);
		copyMethod.visitLookupSwitchInsn( getLabel(dflt), keys, getCopyLabels(labels));
	}


	public void visitMaxs(int maxStack, int maxLocals) {
		originalMethod.visitMaxs(maxStack, maxLocals);
		copyMethod.visitMaxs(maxStack, maxLocals);
	}


	public void visitMethodInsn(int opcode, String owner, String name, String desc) {
		originalMethod.visitMethodInsn(opcode, owner, name, desc);
		copyMethod.visitMethodInsn(opcode, owner, name, desc);
	}

	public void visitMultiANewArrayInsn(String desc, int dims) {
		originalMethod.visitMultiANewArrayInsn(desc, dims);
		copyMethod.visitMultiANewArrayInsn(desc, dims);

	}

	public AnnotationVisitor visitParameterAnnotation(int parameter, String desc, boolean visible) {
		return new MethodAnnotationVisitor( originalMethod.visitParameterAnnotation(parameter, desc, visible),
				copyMethod.visitParameterAnnotation(parameter, desc, visible));
	}


	public void visitTableSwitchInsn(int min, int max, Label dflt, Label[] labels) {
		originalMethod.visitTableSwitchInsn(min, max, dflt, labels);
		copyMethod.visitTableSwitchInsn(min, max, getLabel(dflt), getCopyLabels(labels));
	}

	public void visitTryCatchBlock(Label start, Label end, Label handler, String type) {
		originalMethod.visitTryCatchBlock(start, end, handler, type);
		copyMethod.visitTryCatchBlock(getLabel(start), getLabel(end), getLabel(handler), type);
	}


	public void visitTypeInsn(int opcode, String type) {
		originalMethod.visitTypeInsn(opcode, type);
		copyMethod.visitTypeInsn(opcode, type);

	}

	public void visitVarInsn(int opcode, int var) {
		originalMethod.visitVarInsn(opcode, var);
		copyMethod.visitVarInsn(opcode, var);
	}

	private Label[] getCopyLabels(Label[] labels) {
		Label[] copyLabels = new Label[ labels.length];
		for( int i=0; i<labels.length ;++i) {
			copyLabels[i] = getLabel(labels[i]);
		}
		return copyLabels;
	}

	private Label getLabel( Label label){
		Label duplicateLabel = labelMap.get( label);
		if( duplicateLabel == null) {
			duplicateLabel = new Label();
			labelMap.put(label, duplicateLabel);
		}
		return duplicateLabel;
	}
}
