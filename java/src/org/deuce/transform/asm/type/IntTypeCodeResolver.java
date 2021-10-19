package org.deuce.transform.asm.type;

import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;

public class IntTypeCodeResolver extends TypeCodeResolver {

	public IntTypeCodeResolver(Type type) {
		super(type);
	}

	@Override
	public int loadCode() {
		return Opcodes.ILOAD;
	}

	@Override
	public int returnCode() {
		return Opcodes.IRETURN;
	}

	@Override
	public int storeCode() {
		return Opcodes.ISTORE;
	}

	@Override
	public int nullValueCode() {
		return Opcodes.ICONST_0;
	}
}
