###############################################################################
# 
#  Copyright (c) 2011
#  Lehigh University
#  Computer Science and Engineering Department
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
# 
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
# 
#     * Neither the name of the University of Rochester nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
# 
# 
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
#  POSSIBILITY OF SUCH DAMAGE.

# pull in all global Make variable definitions
-include Makefile.inc

JAVA = javac
JAVAFLAGS += -classpath .:lib/java-getopt-1.0.13.jar

FILENAMES =  LFListHashSet LFListFSet LFListNode
FILENAMES += WFListHashSet WFListFSet WFListNode
FILENAMES += LFArrayHashSet LFArrayFSet LFArrayHashSetOpt
FILENAMES += WFArrayHashSet WFArrayFSet WFArrayOp
FILENAMES += AdaptiveArrayHashSet AdaptiveArrayHashSetOpt

FILENAMES += SOHashSet

FILENAMES += BenchOpsThread CheckingThread ResizingThread
FILENAMES += ISet LFListNode WFListNode
FILENAMES += SetBench

SOURCES   = $(patsubst %, %.java, $(FILENAMES))
OFILES    = $(patsubst %, %.class, $(FILENAMES))
GOAL      = $(OFILES)

# build rules
.PHONY: all clean realclean info

all: info $(GOAL)
	@echo $(GOAL) complete

info:
	@echo "Building with JAVAFLAGS=${JAVAFLAGS}"

# /clean/ deletes everything from the obj directory
clean:
	@rm -f *.class

# /realclean/ also kills the directory and the dependencies
realclean:
	@rm -rf $(OBJDIR)

%.class: %.java
	@echo [${JAVA}] $< "-->" $@
	@$(JAVA) $(JAVAFLAGS) $<

