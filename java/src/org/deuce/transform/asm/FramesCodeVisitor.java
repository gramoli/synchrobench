package org.deuce.transform.asm;


import org.deuce.objectweb.asm.ClassAdapter;
import org.deuce.objectweb.asm.ClassReader;
import org.deuce.objectweb.asm.ClassWriter;
import org.deuce.objectweb.asm.MethodVisitor;
import org.deuce.objectweb.asm.commons.JSRInlinerAdapter;
import org.deuce.transform.Exclude;


/**
 * Adds frames to allow later {@link ClassReader#EXPAND_FRAMES}
 * @author Guy Korland
 * @since 1.0
 */
@Exclude
public class FramesCodeVisitor extends ClassAdapter{

	final static private int JAVA5_VERSION = 49;
	final static private int JAVA6_VERSION = 50;

	public FramesCodeVisitor( String className) {

		super(new CommonClassWriter( ClassWriter.COMPUTE_MAXS | ClassWriter.COMPUTE_FRAMES, className));
	}
	
	@Override
	public void visit(final int version, final int access, final String name,
			final String signature, final String superName, final String[] interfaces) {
		
		if(version == JAVA6_VERSION) // already has frames 
			throw VersionException.INSTANCE;
		
		super.visit(JAVA5_VERSION, access, name, signature, superName, interfaces);
	}
	
	public byte[] visit( byte[] bytes){
		ClassReader cr = new ClassReader(bytes);
		cr.accept(this, 0);
		return ((ClassWriter)super.cv).toByteArray();
	}
	
	@Override
	public MethodVisitor visitMethod(
			final int access,
			final String name,
			final String desc,
			final String signature,
			final String[] exceptions)
	{
		return new JSRInlinerAdapter( super.visitMethod(access, name, desc, signature, exceptions), access, name, desc, signature, exceptions);
	}

	
	@Exclude
	public static class VersionException extends RuntimeException{
		private static final long serialVersionUID = 1L;
		final public static VersionException INSTANCE = new VersionException();
	}
	
	/**
	 * FIXME This is a work around and should be fixed better in the future.
	 * By overriding the getCommonSuperClass we avoid loading 
	 * @author Guy Korland
	 *
	 */
	@Exclude
	private static class CommonClassWriter extends ClassWriter{
		
		public CommonClassWriter(int flags, String className) {
			super(flags);
		}
		@Override
		protected String getCommonSuperClass(final String type1, final String type2)
	    {
			if( type1.equals(type2))
				return type1;
			
			return  "java/lang/Object";
	    }
	}
}
