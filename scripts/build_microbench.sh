#!/bin/sh

# list of stms to test
TARGETS="estm tinystm sequential tiny100 tiny10B tiny098"
# list of lock to test
LOCKS="lock spinlock"
# othe list for lockfree
LFS="lockfree"
# current microbench version
MBENCH_VERSION=0.4.1
# where to store binaries
EXPERIMENT_BIN_DIR=../bin
# make or gmake
MAKE="make"

SRCDIR=../src
BINDIR=../bin
BUILDIR=../build

for TARGET in $TARGETS
do
       printf "\n********************************\n*  Building $app with $TARGET...\n********************************\n\n"
       ${MAKE} -C .. clean
       ${MAKE} -C .. ${TARGET} 
       mv ${BINDIR}/lf-ll ${EXPERIMENT_BIN_DIR}/ll-${MBENCH_VERSION}-${TARGET}
       mv ${BINDIR}/lf-sl ${EXPERIMENT_BIN_DIR}/sl-${MBENCH_VERSION}-${TARGET}
       mv ${BINDIR}/lf-ht ${EXPERIMENT_BIN_DIR}/ht-${MBENCH_VERSION}-${TARGET}
       mv ${BINDIR}/lf-rt ${EXPERIMENT_BIN_DIR}/rt-${MBENCH_VERSION}-${TARGET}
       mv ${BINDIR}/lf-dq ${EXPERIMENT_BIN_DIR}/dq-${MBENCH_VERSION}-${TARGET}
done
for LOCK in $LOCKS
do 
       printf "\n********************************\n*  Building $app with $LOCK...\n********************************\n\n"
       ${MAKE} -C .. clean
       ${MAKE} -C .. ${LOCK} 
       mv ${BINDIR}/lb-ll ${EXPERIMENT_BIN_DIR}/ll-${MBENCH_VERSION}-${LOCK}
       mv ${BINDIR}/lb-sl ${EXPERIMENT_BIN_DIR}/sl-${MBENCH_VERSION}-${LOCK}
       mv ${BINDIR}/lb-ht ${EXPERIMENT_BIN_DIR}/ht-${MBENCH_VERSION}-${LOCK}
done
for LF in $LFS
do 
       printf "\n********************************\n*  Building $app with $LF...\n********************************\n\n"
       ${MAKE} -C .. clean
       ${MAKE} -C .. ${LF} 
       mv ${BINDIR}/lf-ll ${EXPERIMENT_BIN_DIR}/ll-${MBENCH_VERSION}-${LF}
       mv ${BINDIR}/lf-sl ${EXPERIMENT_BIN_DIR}/sl-${MBENCH_VERSION}-${LF}
       mv ${BINDIR}/lf-ht ${EXPERIMENT_BIN_DIR}/ht-${MBENCH_VERSION}-${LF}
done

${MAKE} clean

