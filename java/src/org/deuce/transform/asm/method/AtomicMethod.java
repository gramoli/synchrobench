package org.deuce.transform.asm.method;

import java.util.concurrent.atomic.AtomicInteger;

import org.deuce.Atomic;
import org.deuce.objectweb.asm.AnnotationVisitor;
import org.deuce.objectweb.asm.Label;
import org.deuce.objectweb.asm.MethodAdapter;
import org.deuce.objectweb.asm.MethodVisitor;
import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;
import org.deuce.objectweb.asm.commons.Method;
import org.deuce.transaction.AbortTransactionException;
import org.deuce.transaction.Context;
import org.deuce.transaction.ContextDelegator;
import org.deuce.transaction.TransactionException;
import org.deuce.transform.asm.type.TypeCodeResolver;
import org.deuce.transform.asm.type.TypeCodeResolverFactory;

public class AtomicMethod extends MethodAdapter implements Opcodes{

	final static public String ATOMIC_DESCRIPTOR = Type.getDescriptor(Atomic.class);
	final static private AtomicInteger ATOMIC_BLOCK_COUNTER = new AtomicInteger(0);
	
	private Integer retries = Integer.getInteger("org.deuce.transaction.retries", Integer.MAX_VALUE);
	private String metainf = "";//Integer.getInteger("org.deuce.transaction.retries", Integer.MAX_VALUE);
	
	final private String className;
	final private String methodName;
	final private TypeCodeResolver returnReolver;
	final private TypeCodeResolver[] argumentReolvers;
	final private boolean isStatic;
	final private int variablesSize;
	final private Method newMethod;
	
	public AtomicMethod(MethodVisitor mv, String className, String methodName,
			String descriptor, Method newMethod, boolean isStatic) {
		super(mv);
		this.className = className;
		this.methodName = methodName;
		this.newMethod = newMethod;
		this.isStatic = isStatic;

		Type returnType = Type.getReturnType(descriptor);
		Type[] argumentTypes = Type.getArgumentTypes(descriptor);

		returnReolver = TypeCodeResolverFactory.getReolver(returnType);
		argumentReolvers = new TypeCodeResolver[ argumentTypes.length];
		for( int i=0; i< argumentTypes.length ; ++i) {
			argumentReolvers[ i] = TypeCodeResolverFactory.getReolver( argumentTypes[ i]);
		}
		variablesSize = variablesSize( argumentReolvers, isStatic);
	}
	

	@Override
	public AnnotationVisitor visitAnnotation(String desc, boolean visible) {
		final AnnotationVisitor visitAnnotation = super.visitAnnotation(desc, visible);
		if( AtomicMethod.ATOMIC_DESCRIPTOR.equals(desc)){
			return new AnnotationVisitor(){
				public void visit(String name, Object value) {
					if( name.equals("retries"))
						AtomicMethod.this.retries = (Integer)value;
					
					if( name.equals("metainf"))
						AtomicMethod.this.metainf = (String)value;
					
					visitAnnotation.visit(name, value);
				}
				public AnnotationVisitor visitAnnotation(String name, String desc) {
					return visitAnnotation.visitAnnotation(name, desc);
				}
				public AnnotationVisitor visitArray(String name) {
					return visitAnnotation.visitArray(name);
				}
				public void visitEnd() {
					visitAnnotation.visitEnd();				
				}
				public void visitEnum(String name, String desc, String value) {
					visitAnnotation.visitEnum(name, desc, value);
				}
			};
		}
		return visitAnnotation;
	}

	/**
	public static boolean foo(Object s) throws IOException{

		Throwable throwable = null;
		Context context = ContextDelegator.getInstance();
		boolean commit = true;
		boolean result = true;
		for( int i=10 ; i>0 ; --i)
		{
			context.init(atomicBlockId, metainf);
			try
			{
				result = foo(s,context);
			}
			catch( AbortTransactionException ex)
			{
				context.rollback(); 
				throw ex;
			}
			catch( TransactionException ex)
			{
				commit = false;
			}
			catch( Throwable ex)
			{
				throwable = ex;
			}

			if( commit )
			{
				if( context.commit()){
					if( throwable != null)
						throw (IOException)throwable;
					return result;
				}
			}
			else
			{
				context.rollback(); 
				commit = true;
			}
		}
		throw new TransactionException();

	}
	 */
	@Override
	public void visitCode() {

		final int indexIndex = variablesSize; // i
		final int contextIndex = indexIndex + 1; // context
		final int throwableIndex = contextIndex + 1;
		final int commitIndex = throwableIndex + 1;
		final int exceptionIndex = commitIndex + 1;
		final int resultIndex = exceptionIndex + 1;

		Label l0 = new Label();
		Label l1 = new Label();
		Label l25 = new Label();
		mv.visitTryCatchBlock(l0, l1, l25, AbortTransactionException.ABORT_TRANSACTION_EXCEPTION_INTERNAL);  // try{
		Label l2 = new Label();
		mv.visitTryCatchBlock(l0, l1, l2, TransactionException.TRANSACTION_EXCEPTION_INTERNAL);  // try{ 
		Label l3 = new Label();
		mv.visitTryCatchBlock(l0, l1, l3, Type.getInternalName( Throwable.class));  // try{
		
		Label l4 = new Label(); // Throwable throwable = null;
		mv.visitLabel(l4);
		mv.visitInsn(ACONST_NULL);
		mv.visitVarInsn(ASTORE, throwableIndex);
		
		Label l5 = getContext(contextIndex); // Context context = ContextDelegator.getInstance();
			
		Label l6 = new Label(); // boolean commit = true;
		mv.visitLabel(l6);
		mv.visitInsn(ICONST_1);
		mv.visitVarInsn(ISTORE, commitIndex);
		
		Label l7 = new Label(); // ... result = null;
		mv.visitLabel(l7);
		if( returnReolver != null)
		{
			mv.visitInsn( returnReolver.nullValueCode());
			mv.visitVarInsn( returnReolver.storeCode(), resultIndex);
		}
		
		Label l8 = new Label(); // for( int i=10 ; ... ; ...)
		mv.visitLabel(l8);
		mv.visitLdcInsn( retries);
		mv.visitVarInsn(ISTORE, indexIndex);
		
		Label l9 = new Label();
		mv.visitLabel(l9);
		Label l10 = new Label();
		mv.visitJumpInsn(GOTO, l10);
		
		Label l11 = new Label(); // context.init(atomicBlockId, metainf);
		mv.visitLabel(l11);
		mv.visitVarInsn(ALOAD, contextIndex);
		mv.visitLdcInsn(ATOMIC_BLOCK_COUNTER.getAndIncrement());
		mv.visitLdcInsn(metainf);
		mv.visitMethodInsn(INVOKEINTERFACE, Context.CONTEXT_INTERNAL, "init", "(ILjava/lang/String;)V");
		
		/* result = foo( context, ...)  */ 
		mv.visitLabel(l0);
		if( !isStatic) // load this id if not static
			mv.visitVarInsn(ALOAD, 0);

		// load the rest of the arguments
		int local = isStatic ? 0 : 1;
		for( int i=0 ; i < argumentReolvers.length ; ++i) { 
			mv.visitVarInsn(argumentReolvers[i].loadCode(), local);
			local += argumentReolvers[i].localSize(); // move to the next argument
		}
		
		mv.visitVarInsn(ALOAD, contextIndex); // load the context
		
		if( isStatic)
			mv.visitMethodInsn(INVOKESTATIC, className, methodName, newMethod.getDescriptor()); // ... = foo( ...
		else
			mv.visitMethodInsn(INVOKEVIRTUAL, className, methodName, newMethod.getDescriptor()); // ... = foo( ...

		if( returnReolver != null) 
			mv.visitVarInsn(returnReolver.storeCode(), resultIndex); // result = ...
		
		mv.visitLabel(l1);
		Label l12 = new Label();
		mv.visitJumpInsn(GOTO, l12);

		/*catch( AbortTransactionException ex)
		{
			throw ex;
		}*/
		mv.visitLabel(l25);
		mv.visitVarInsn(ASTORE, exceptionIndex);
		Label l27 = new Label();
		mv.visitVarInsn(ALOAD, contextIndex);
		mv.visitMethodInsn(INVOKEINTERFACE, Context.CONTEXT_INTERNAL, "rollback", "()V");
		mv.visitLabel(l27);
		mv.visitVarInsn(ALOAD, exceptionIndex);
		mv.visitInsn(ATHROW);
		Label l28 = new Label();
		mv.visitLabel(l28);
		mv.visitJumpInsn(GOTO, l12);
		
		/*catch( TransactionException ex)
		{
			commit = false;
		}*/
		mv.visitLabel(l2);
		mv.visitVarInsn(ASTORE, exceptionIndex);
		Label l13 = new Label();
		mv.visitLabel(l13);
		mv.visitInsn(ICONST_0);
		mv.visitVarInsn(ISTORE, commitIndex);
		Label l14 = new Label();
		mv.visitLabel(l14);
		mv.visitJumpInsn(GOTO, l12);
		
		/*catch( Throwable ex)
		{
			throwable = ex;
		}*/
		mv.visitLabel(l3);
		mv.visitVarInsn(ASTORE, exceptionIndex);
		Label l15 = new Label();
		mv.visitLabel(l15);
		mv.visitVarInsn(ALOAD, exceptionIndex);
		mv.visitVarInsn(ASTORE, throwableIndex);
		
		/*
		 * if( commit )
			{
				if( context.commit()){
					if( throwable != null)
						throw (IOException)throwable;
					return result;
				}
			}
			else
			{
				context.rollback(); 
				commit = true;
			}
		 */
		mv.visitLabel(l12); // if( commit )
		mv.visitVarInsn(ILOAD, commitIndex);
		Label l16 = new Label();
		mv.visitJumpInsn(IFEQ, l16);
		
		Label l17 = new Label(); // if( context.commit())
		mv.visitLabel(l17);
		mv.visitVarInsn(ALOAD, contextIndex);
		mv.visitMethodInsn(INVOKEINTERFACE, Context.CONTEXT_INTERNAL, "commit", "()Z");
		Label l18 = new Label();
		mv.visitJumpInsn(IFEQ, l18);
		
		//		if( throwable != null)
		//			throw throwable;
		Label l19 = new Label();
		mv.visitLabel(l19);
		mv.visitVarInsn(ALOAD, throwableIndex);
		Label l20 = new Label();
		mv.visitJumpInsn(IFNULL, l20);
		Label l21 = new Label();
		mv.visitLabel(l21);
		mv.visitVarInsn(ALOAD, throwableIndex);
		mv.visitInsn(ATHROW);
		
		// return
		mv.visitLabel(l20);
		if( returnReolver == null) {
			mv.visitInsn( RETURN); // return;
		}
		else {
			mv.visitVarInsn(returnReolver.loadCode(), resultIndex); // return result;
			mv.visitInsn(returnReolver.returnCode());
		}
		
		mv.visitJumpInsn(GOTO, l18);
		
		// else
		mv.visitLabel(l16); // context.rollback(); 
		mv.visitVarInsn(ALOAD, contextIndex);
		mv.visitMethodInsn(INVOKEINTERFACE, Context.CONTEXT_INTERNAL, "rollback", "()V");
		
		mv.visitInsn(ICONST_1); // commit = true;
		mv.visitVarInsn(ISTORE, commitIndex);
		
		mv.visitLabel(l18);  // for( ... ; i>0 ; --i) 
		mv.visitIincInsn(indexIndex, -1);
		mv.visitLabel(l10);
		mv.visitVarInsn(ILOAD, indexIndex);
		mv.visitJumpInsn(IFGT, l11);
		
		// throw new TransactionException("Failed to commit ...");
		Label l23 = throwTransactionException();
		
		/* locals */
		Label l24 = new Label();
		mv.visitLabel(l24);
		mv.visitLocalVariable("throwable", "Ljava/lang/Throwable;", null, l5, l24, throwableIndex);
		mv.visitLocalVariable("context", Context.CONTEXT_DESC, null, l6, l24, contextIndex);
		mv.visitLocalVariable("commit", "Z", null, l7, l24, commitIndex);
		if( returnReolver != null)
			mv.visitLocalVariable("result", returnReolver.toString(), null, l8, l24, resultIndex);
		mv.visitLocalVariable("i", "I", null, l9, l23, indexIndex);
		mv.visitLocalVariable("ex", "Lorg/deuce/transaction/AbortTransactionException;", null, l27, l28, exceptionIndex);
		mv.visitLocalVariable("ex", "Lorg/deuce/transaction/TransactionException;", null, l13, l14, exceptionIndex);
		mv.visitLocalVariable("ex", "Ljava/lang/Throwable;", null, l15, l12, exceptionIndex);
		
		mv.visitMaxs(6 + variablesSize, resultIndex + 2);
		mv.visitEnd();
	}

	private Label getContext(final int contextIndex) {
		Label label = new Label();
		mv.visitLabel(label); // Context context = ContextDelegator.getInstance();
		mv.visitMethodInsn(INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL, "getInstance", "()Lorg/deuce/transaction/Context;");
		mv.visitVarInsn(ASTORE, contextIndex);
		return label;
	}

	private Label throwTransactionException() {
		Label label = new Label();
		mv.visitLabel(label);
		mv.visitTypeInsn(NEW, "org/deuce/transaction/TransactionException");
		mv.visitInsn(DUP);
		mv.visitLdcInsn("Failed to commit the transaction in the defined retries.");
		mv.visitMethodInsn(INVOKESPECIAL, "org/deuce/transaction/TransactionException", "<init>", "(Ljava/lang/String;)V");
		mv.visitInsn(ATHROW);
		return label;
	}

	@Override
	public void visitFrame(int type, int local, Object[] local2, int stack, Object[] stack2) {
	}

	@Override
	public void visitIincInsn(int var, int increment) {
	}

	@Override
	public void visitInsn(int opcode) {
	}

	@Override
	public void visitIntInsn(int opcode, int operand) {
	}

	@Override
	public void visitJumpInsn(int opcode, Label label) {
	}

	@Override
	public void visitLabel(Label label) {
	}

	@Override
	public void visitEnd() {
	}

	@Override
	public void visitFieldInsn(int opcode, String owner, String name, String desc) {
	}


	@Override
	public void visitLdcInsn(Object cst) {
	}

	@Override
	public void visitLineNumber(int line, Label start) {
	}

	@Override
	public void visitLocalVariable(String name, String desc, String signature, Label start,
			Label end, int index) {
	}

	@Override
	public void visitLookupSwitchInsn(Label dflt, int[] keys, Label[] labels) {
	}

	@Override
	public void visitMaxs(int maxStack, int maxLocals) {
	}

	@Override
	public void visitMethodInsn(int opcode, String owner, String name, String desc) {
	}

	@Override
	public void visitMultiANewArrayInsn(String desc, int dims) {
	}

	@Override
	public void visitTableSwitchInsn(int min, int max, Label dflt, Label[] labels) {
	}

	@Override
	public void visitTryCatchBlock(Label start, Label end, Label handler, String type) {
	}

	@Override
	public void visitTypeInsn(int opcode, String type) {
	}

	@Override
	public void visitVarInsn(int opcode, int var) {
	}

	public void setRetries(int retries) {
		this.retries = retries;
	}

	private int variablesSize( TypeCodeResolver[] types, boolean isStatic) {
		int i = isStatic ? 0 : 1;
		for( TypeCodeResolver type : types) {
			i += type.localSize();
		}
		return i;
	}
}
