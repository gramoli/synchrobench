package org.deuce.transform.util;

public class Util {
	final static private String ADDRESS_FIELD_POSTFIX = "__ADDRESS__";
	final static private String OBJECT_FIELD_POSTFIX = "__OBJECT__";
	final static private String OBJECT_ADDRESS_FIELD_POSTFIX = "__OBJECT_ADDRESS__";

	public static String getAddressField(String field) {
		return field + ADDRESS_FIELD_POSTFIX;
	}

	public static String getObjectField(String field) {
		return field + OBJECT_FIELD_POSTFIX;
	}

	public static String getObjectAddressField(String field) {
		return field + OBJECT_ADDRESS_FIELD_POSTFIX;
	}
}
