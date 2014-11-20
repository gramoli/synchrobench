package org.deuce.transform.asm.type;

import org.deuce.objectweb.asm.Opcodes;
import org.deuce.objectweb.asm.Type;

public class FloatTypeCodeResolver extends TypeCodeResolver {

	public FloatTypeCodeResolver(Type type) {
		super(type);
	}

	@Override
	public int loadCode() {
		return Opcodes.FLOAD;
	}

	@Override
	public int returnCode() {
		return Opcodes.FRETURN;
	}

	@Override
	public int storeCode() {
		return Opcodes.FSTORE;
	}

	@Override
	public int nullValueCode() {
		return Opcodes.FCONST_0;
	}
}
