package org.deuce.transform.asm.method;

import org.deuce.objectweb.asm.Label;
import org.deuce.objectweb.asm.MethodAdapter;
import org.deuce.objectweb.asm.MethodVisitor;
import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;
import org.deuce.objectweb.asm.commons.AnalyzerAdapter;
import org.deuce.objectweb.asm.commons.Method;
import org.deuce.transaction.Context;
import org.deuce.transaction.ContextDelegator;
import org.deuce.transaction.strongiso.field.ArrayReadFieldAccess;
import org.deuce.transform.UseStrongIso;
import org.deuce.transform.asm.ClassTransformer;
import org.deuce.transform.asm.ExcludeIncludeStore;
import org.deuce.transform.asm.FieldsHolder;
import org.deuce.transform.util.Util;

public class DuplicateMethod extends MethodAdapter{

	final static public String LOCAL_VARIBALE_NAME = "__transactionContext__";

	private final int argumentsSize;
	private final FieldsHolder fieldsHolder;
	
	private Label firstLabel;
	private Label lastLabel;
	private boolean addContextToTable = false;
	private AnalyzerAdapter analyzerAdapter;

	public DuplicateMethod(MethodVisitor mv, boolean isstatic, Method newMethod, FieldsHolder fieldsHolder) {
		super(mv);
		this.fieldsHolder = fieldsHolder;
		this.argumentsSize = calcArgumentsSize( isstatic, newMethod); 
	}
	
	public void setAnalyzer(AnalyzerAdapter analyzerAdapter) {
		this.analyzerAdapter = analyzerAdapter;
	}

	@Override
	public void visitMethodInsn(int opcode, String owner, String name,
			String desc) 
	{
		if( ExcludeIncludeStore.exclude(owner))
		{
			super.visitMethodInsn(opcode, owner, name, desc); // ... = foo( ...
		}
		else
		{
			super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
			Method newMethod = ClassTransformer.createNewMethod(name, desc);
			super.visitMethodInsn(opcode, owner, name, newMethod.getDescriptor()); // ... = foo( ...
		}
	}
	
	
	/**
	 * Adds for each field visited a call to the context.
	 */
	@Override
	public void visitFieldInsn(int opcode, String owner, String name, String desc) {
		if( ExcludeIncludeStore.exclude( owner) || 
				name.contains("$")){ // Syntactic TODO remove this limitation
			super.visitFieldInsn(opcode, owner, name, desc); // ... = foo( ...
			return;
		}
		
		String fieldsHolderName = fieldsHolder.getFieldsHolderName(owner);
		mv.visitFieldInsn(Opcodes.GETSTATIC, fieldsHolderName, Util.getAddressField(name), "J");
		Label l1 = new Label();
		mv.visitInsn(Opcodes.LCONST_0);
		mv.visitInsn(Opcodes.LCMP);
		mv.visitJumpInsn(Opcodes.IFGE, l1);
		super.visitFieldInsn(opcode, owner, name, desc);
		Label l2 = new Label();
		mv.visitJumpInsn(Opcodes.GOTO, l2);
		mv.visitLabel(l1);
		
		final Type type = Type.getType(desc);
		switch( opcode) {
		case Opcodes.GETFIELD:  //	ALOAD 0: this (stack status)
			
			addBeforeReadCall(fieldsHolderName, opcode, owner, name, desc);
			
			super.visitInsn(Opcodes.DUP);
			super.visitFieldInsn(opcode, owner, name, desc);
			super.visitFieldInsn( Opcodes.GETSTATIC, fieldsHolderName, Util.getAddressField(name) , "J");
			super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
			super.visitMethodInsn( Opcodes.INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL,
					ContextDelegator.READ_METHOD_NAME, ContextDelegator.getReadMethodDesc(type));
			
			if( type.getSort() >= Type.ARRAY) // non primitive
				super.visitTypeInsn( Opcodes.CHECKCAST, Type.getType(desc).getInternalName());
			break;
		case Opcodes.PUTFIELD:
			super.visitFieldInsn( Opcodes.GETSTATIC, fieldsHolderName, Util.getAddressField(name) , "J");
			if(UseStrongIso.USE_STRONG_ISO)
				super.visitFieldInsn( Opcodes.GETSTATIC, fieldsHolderName, Util.getObjectAddressField(name) , "J");
			super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
			super.visitMethodInsn( Opcodes.INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL,
					ContextDelegator.WRITE_METHOD_NAME, ContextDelegator.getWriteMethodDesc(type));
			break;
		case Opcodes.GETSTATIC: // check support for static fields
			super.visitFieldInsn(Opcodes.GETSTATIC, fieldsHolderName, 
					StaticMethodTransformer.CLASS_BASE, "Ljava/lang/Object;");
			
			addBeforeReadCall(fieldsHolderName, opcode, owner, name, desc);
			
			super.visitFieldInsn(opcode, owner, name, desc);
			super.visitFieldInsn(Opcodes.GETSTATIC, fieldsHolderName, Util.getAddressField(name) , "J");
			super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
			super.visitMethodInsn( Opcodes.INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL,
					ContextDelegator.READ_METHOD_NAME, ContextDelegator.getReadMethodDesc(type));
			
			if( type.getSort() >= Type.ARRAY) // non primitive
				super.visitTypeInsn( Opcodes.CHECKCAST, Type.getType(desc).getInternalName());
			break;
		case Opcodes.PUTSTATIC:
			super.visitFieldInsn(Opcodes.GETSTATIC, fieldsHolderName, 
					StaticMethodTransformer.CLASS_BASE, "Ljava/lang/Object;");
			super.visitFieldInsn( Opcodes.GETSTATIC, fieldsHolderName, Util.getAddressField(name) , "J");
			if(UseStrongIso.USE_STRONG_ISO)
				super.visitFieldInsn( Opcodes.GETSTATIC, fieldsHolderName, Util.getObjectAddressField(name) , "J");
			super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
			super.visitMethodInsn( Opcodes.INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL,
					ContextDelegator.STATIC_WRITE_METHOD_NAME, ContextDelegator.getStaticWriteMethodDesc(type));
			break;
		default:
			super.visitFieldInsn(opcode, owner, name, desc);
		}
		mv.visitLabel(l2);
	}

	private void addBeforeReadCall(String fieldsHolderName, int opcode, String owner, String name, String desc) {
		//super.visitInsn(Opcodes.DUP);
		super.visitInsn(Opcodes.DUP);
		if(UseStrongIso.USE_STRONG_ISO)
			super.visitInsn(Opcodes.DUP);
		super.visitFieldInsn( Opcodes.GETSTATIC, fieldsHolderName, Util.getAddressField(name) , "J");
		if(UseStrongIso.USE_STRONG_ISO) {
			// FIXME Dont need to do this because not doing the arrays right anyway
			UseStrongIso.owner = fieldsHolderName;
			UseStrongIso.name = name;
			UseStrongIso.desc = desc;
			//super.visitFieldInsn( Opcodes.GETFIELD, fieldsHolderName, name , desc);
			//super.visitInsn(Opcodes.DUP);
			super.visitFieldInsn( Opcodes.GETSTATIC, fieldsHolderName, Util.getObjectAddressField(name) , "J");
		}
		super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
		super.visitMethodInsn( Opcodes.INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL,
				ContextDelegator.BEFORE_READ_METHOD_NAME, ContextDelegator.BEFORE_READ_METHOD_DESC);
	}

	/**
	 * Adds for each array cell visited a call to the context
	 */
	@Override
	public void visitInsn(int opcode) {
		boolean load = false;
		boolean store = false;
		String desc = null;
		String arrayMemeberType = null;
		switch( opcode) {
		
		case Opcodes.AALOAD:
			// handle Object[] arrays type, the type before the last is the array. 
			// The substring removes the '[' from the array type

			String arrayType = 
				(String)this.analyzerAdapter.stack.get(this.analyzerAdapter.stack.size() - 2);
			arrayMemeberType = getArrayMemberType( arrayType);
			desc = ContextDelegator.READ_ARRAY_METHOD_OBJ_DESC;
			load = true;
			break;
		case Opcodes.BALOAD:
			desc = ContextDelegator.READ_ARRAY_METHOD_BYTE_DESC;
			load = true;
			break;
		case Opcodes.CALOAD:
			desc = ContextDelegator.READ_ARRAY_METHOD_CHAR_DESC;
			load = true;
			break;
		case Opcodes.SALOAD:
			desc = ContextDelegator.READ_ARRAY_METHOD_SHORT_DESC;
			load = true;
			break;
		case Opcodes.IALOAD:
			desc = ContextDelegator.READ_ARRAY_METHOD_INT_DESC;
			load = true;
			break;
		case Opcodes.LALOAD:
			desc = ContextDelegator.READ_ARRAY_METHOD_LONG_DESC;
			load = true;
			break;
		case Opcodes.FALOAD:
			desc = ContextDelegator.READ_ARRAY_METHOD_FLOAT_DESC;
			load = true;
			break;
		case Opcodes.DALOAD:
			desc = ContextDelegator.READ_ARRAY_METHOD_DOUBLE_DESC;
			load = true;
			break;
			
		case Opcodes.AASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_OBJ_DESC;
			store = true;
			break;
		case Opcodes.BASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_BYTE_DESC;
			store = true;
			break;
		case Opcodes.CASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_CHAR_DESC;
			store = true;
			break;
		case Opcodes.SASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_SHORT_DESC;
			store = true;
			break;
		case Opcodes.IASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_INT_DESC;
			store = true;
			break;
		case Opcodes.LASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_LONG_DESC;
			store = true;
			break;
		case Opcodes.FASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_FLOAT_DESC;
			store = true;
			break;
		case Opcodes.DASTORE:
			desc = ContextDelegator.WRITE_ARRAY_METHOD_DOUBLE_DESC;
			store = true;
			break;
		}
			
		if( load)
		{
			if(UseStrongIso.USE_STRONG_ISO) {
				String sisoDesc = Type.getType(ArrayReadFieldAccess.class).getDescriptor();
				//super.visitFieldInsn( Opcodes.GETFIELD, UseStrongIso.owner, Util.getObjectField(UseStrongIso.name) , sisoDesc);
				// FIXME THIS IS WRONG, HOW TO DO ARRAYS????
				super.visitFieldInsn( Opcodes.GETSTATIC, UseStrongIso.owner, Util.getObjectAddressField(UseStrongIso.name) , "J");
			}
			super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
			super.visitMethodInsn( Opcodes.INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL,
					ContextDelegator.READ_ARR_METHOD_NAME, desc);

			if( opcode == Opcodes.AALOAD){ // non primitive array need cast
				super.visitTypeInsn( Opcodes.CHECKCAST, arrayMemeberType);
			}
		}
		else if( store)
		{
			super.visitVarInsn(Opcodes.ALOAD, argumentsSize - 1); // load context
			super.visitMethodInsn( Opcodes.INVOKESTATIC, ContextDelegator.CONTEXT_DELEGATOR_INTERNAL,
					ContextDelegator.WRITE_ARR_METHOD_NAME, desc);
		}
		else{
			super.visitInsn(opcode);
		}
	}

	@Override
	public void visitIincInsn(int var, int increment) {
		super.visitIincInsn( newIndex(var), increment); // increase index due to context
	}

	@Override
	public void visitLabel(Label label) {
		if( firstLabel == null)
			firstLabel = label;
		lastLabel = label;
		super.visitLabel(label);
	}

	@Override
	public void visitLocalVariable(String name, String desc, String signature, Label start,
			Label end, int index) {
		if( this.argumentsSize >  index + 1) // argument
		{
			super.visitLocalVariable(name, desc, signature, start, end, index); // non static method has this
			return;
		}
		// add context as last argument
		// the first local variable and was never added before
		if( this.argumentsSize ==  index + 1 && !addContextToTable) 
		{
			addContextToTable = true;
			super.visitLocalVariable(LOCAL_VARIBALE_NAME, Context.CONTEXT_DESC, null,
					firstLabel, lastLabel, index);
		}

		// increase all the locals index
		super.visitLocalVariable(name, desc, signature, start, end, index + 1);
	}

	@Override
	public void visitMaxs(int maxStack, int maxLocals) {
		super.visitMaxs(maxStack + 3, maxLocals + 1);
	}

	@Override
	public void visitVarInsn(int opcode, int var) {	
		// increase the local variable index by 1
		super.visitVarInsn(opcode, newIndex(var));  
	}
	
	/**
	 * Calculate the new local index according to its position.
	 * If it's not a function argument (local variable) its index increased by 1.
	 * @param currIndex current index
	 * @return new index
	 */
	private int newIndex( int currIndex){
		return currIndex + 1 < this.argumentsSize ? currIndex : currIndex + 1;
	}
	
	private int calcArgumentsSize( boolean isStatic, Method newMethod){
		int size = isStatic ? 0 : 1; // if not static "this" is the first argument
		for( Type type : newMethod.getArgumentTypes()){
			size += type.getSize();
		}
		return size;
	}
	
	private String getArrayMemberType( String arrayType){
		if( arrayType.charAt(arrayType.length() - 1) == ';' && // primitive array 
				arrayType.charAt(1) != '[' ) // array of arrays  
			return arrayType.substring(2, arrayType.length()-1);

		return arrayType.substring(1, arrayType.length()); // array of Objects
	}
}
