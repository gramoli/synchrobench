package org.deuce;

import static java.lang.annotation.ElementType.METHOD;
import static java.lang.annotation.RetentionPolicy.CLASS;

import java.lang.annotation.Retention;
import java.lang.annotation.Target;

/**
* Used to mark a method as unsafe, in this case a transaction context will be suspended untill this method returns. 
* @author	Guy Korland
* @since 1.4
*/
@Target(METHOD)
@Retention(CLASS)
public @interface Unsafe {

}
