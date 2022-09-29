XJDK_HOME	?= .
AGENT  		?= lib/deuceAgent-1.3.0.jar
JUNIT		?= lib/junit.jar
JAVA 		?= java
JAVAC		?= javac
JAVAOPT 	?= -server 

BIN	?= ${XJDK_HOME}/bin

STM     ?= estmmvcc
ifndef STM
  SYNC = global
else
  SYNC = ${STM}.Context
endif

ANT=ant

JDK_RT ?= /opt/jdk1.8.0_172/jre/lib/rt.jar

#############################
# Agent
#############################
JAR_STM = mydeuce.jar
EXCLUDE	= -Dorg.deuce.exclude=java.lang.Enum,sun.*
MAIN   	= org.deuce.transform.asm.Agent

#############################
# Benchmark
#############################
VERSION	   = 0.1
JAR_BENCH  = compositional-deucestm-${VERSION}.jar
MAIN_CLASS = contention/benchmark/Test.java

CP 	= ${BIN}:${AGENT}

#############################
# Bytecode archives
#############################
JAR_DIR ?= lib
BENCH   =  ${JAR_DIR}/compositional-deucestm-${VERSION}.jar
RT      =  ${JAR_DIR}/rt_instrumented.jar
MYDEUCE	=  ${JAR_DIR}/${JAR_STM}

all: compile instrument

instrument:
	${JAVA} -cp ${AGENT} ${EXCLUDE} ${MAIN} ${JDK_RT} ${RT}

compile:
	${ANT} 
	${ANT} jarbench
	${ANT} jardeuce 

compilev:
	${ANT} -v
	${ANT} -v jarbench
	${ANT} -v jardeuce

check:
	${JAVA} ${JAVAOPT} -Dorg.deuce.exclude="java.util.*,java.lang.*,sun.*" \
	-javaagent:${AGENT} \
	-Dorg.deuce.transaction.contextClass=org.deuce.transaction.${SYNC} \
	-cp ${BENCH}:${MYDEUCE}:bin \
	-Xbootclasspath/p:${RT} \
	contention.benchmark.Test \
	-v -W 2 -u 20 -a 0 -s 0 -d 2000 -t 64 -i 4096 -r 8192 -b linkedlists.transactional.ReusableLinkedListIntSet

check-notx:
	${JAVA} ${JAVAOPT} \
	-cp bin \
	contention.benchmark.Test \
	-W 0 -w 20 -a 0 -s 0 -l 2000 -t 1 -i 4096 -r 8192 -b linkedlists.lockbased.LazyLinkedListSortedSet 

clean:
	rm -rf bin/*
	rm ${BENCH} ${MYDEUCE} ${RT}

