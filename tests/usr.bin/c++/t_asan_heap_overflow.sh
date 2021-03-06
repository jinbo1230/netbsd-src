#	$NetBSD: t_asan_heap_overflow.sh,v 1.2 2018/07/16 07:27:26 kamil Exp $
#
# Copyright (c) 2018 The NetBSD Foundation, Inc.
# All rights reserved.
#
# This code is derived from software contributed to The NetBSD Foundation
# by Siddharth Muralee.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#

SUPPORT='n'
test_target() {
	if uname -m | grep -q "amd64"; then
		SUPPORT='y'
	fi

	if uname -m | grep -q "i386"; then
		SUPPORT='y'
	fi
}

atf_test_case heap_overflow
heap_overflow_head() {
	atf_set "descr" "compile and run \"Heap Overflow example\""
	atf_set "require.progs" "c++ paxctl"
}

atf_test_case heap_overflow_profile
heap_overflow_profile_head() {
	atf_set "descr" "compile and run \"Heap Overflow example\" with profiling option"
	atf_set "require.progs" "c++ paxctl"
}

atf_test_case heap_overflow_pic
heap_overflow_pic_head() {
	atf_set "descr" "compile and run PIC \"Heap Overflow example\""
	atf_set "require.progs" "c++ paxctl"
}

atf_test_case heap_overflow_pie
heap_overflow_pie_head() {
	atf_set "descr" "compile and run position independent (PIE) \"Heap Overflow example\""
	atf_set "require.progs" "c++ paxctl"
}

atf_test_case heap_overflow32
heap_overflow32_head() {
	atf_set "descr" "compile and run \"Heap Overflow example\" for/in netbsd32 emulation"
	atf_set "require.progs" "c++ paxctl file diff cat"
}

atf_test_case target_not_supported
target_not_supported_head()
{
	atf_set "descr" "Test forced skip"
}

heap_overflow_body() {
	cat > test.cpp << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int foo(int index) { int *x = (int *)malloc(20); int res = x[index * 4]; free(x); return res;}
int main(int argc, char **argv) {foo(argc + 19); printf("CHECK\n"); exit(0);}
EOF
	c++ -fsanitize=address -o test test.cpp
	paxctl -a test
	atf_check -s not-exit:0 -o not-match:"CHECK\n" -e match:"heap-buffer-overflow" ./test
}

heap_overflow_profile_body() {
	cat > test.cpp << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int foo(int index) { int *x = (int *)malloc(20); int res = x[index * 4]; free(x); return res;}
int main(int argc, char **argv) {foo(argc + 19); printf("CHECK\n"); exit(0);}
EOF
	c++ -fsanitize=address -o test -pg test.cpp
	paxctl +a test
	atf_check -s not-exit:0 -o not-match:"CHECK\n" -e match:"heap-buffer-overflow" ./test
}

heap_overflow_pic_body() {
	cat > test.cpp << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int foo(int);
int main(int argc, char **argv) {foo(argc + 19); printf("CHECK\n"); exit(0);}
EOF
	cat > pic.cpp << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int foo(int index) { int *x = (int *)malloc(20); int res = x[index * 4]; free(x); return res;}
EOF

	c++ -fPIC -fsanitize=address -shared -o libtest.so pic.cpp
	c++ -o test test.cpp -fsanitize=address -L. -ltest
	paxctl +a test

	export LD_LIBRARY_PATH=.
	atf_check -s not-exit:0 -o not-match:"CHECK\n" -e match:"heap-buffer-overflow" ./test
}

heap_overflow_pie_body() {
	# check whether this arch supports -pice
	if ! c++ -pie -dM -E - < /dev/null 2>/dev/null >/dev/null; then
		atf_set_skip "c++ -pie not supported on this architecture"
	fi
	cat > test.cpp << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int foo(int index) { int *x = (int *)malloc(20); int res = x[index * 4]; free(x); return res;}
int main(int argc, char **argv) {foo(argc + 19); printf("CHECK\n"); exit(0);}
EOF
	c++ -fsanitize=address -fpie -pie -o test test.cpp
	paxctl +a test
	atf_check -s not-exit:0 -o not-match:"CHECK\n" -e match:"heap-buffer-overflow" ./test
}

heap_overflow32_body() {
	# check whether this arch is 64bit
	if ! c++ -dM -E - < /dev/null | fgrep -q _LP64; then
		atf_skip "this is not a 64 bit architecture"
	fi
	if ! c++ -m32 -dM -E - < /dev/null 2>/dev/null > ./def32; then
		atf_skip "c++ -m32 not supported on this architecture"
	else
		if fgrep -q _LP64 ./def32; then
		atf_fail "c++ -m32 does not generate netbsd32 binaries"
	fi
fi

	cat > test.cpp << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int foo(int index) { int *x = (int *)malloc(20); int res = x[index * 4]; free(x); return res;}
int main(int argc, char **argv) {foo(argc + 19); printf("CHECK\n"); exit(0);}
EOF
	c++ -fsanitize=address -o ho32 -m32 test.cpp
	c++ -fsanitize=address -o ho64 test.cpp
	file -b ./ho32 > ./ftype32
	file -b ./ho64 > ./ftype64
	if diff ./ftype32 ./ftype64 >/dev/null; then
		atf_fail "generated binaries do not differ"
	fi
	echo "32bit binaries on this platform are:"
	cat ./ftype32
	echo "While native (64bit) binaries are:"
	cat ./ftype64
	paxctl +a ho32
	atf_check -s not-exit:0 -o not-match:"CHECK\n" -e match:"heap-buffer-overflow" ./ho32

# and another test with profile 32bit binaries
	cat > test.cpp << EOF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int foo(int index) { int *x = (int *)malloc(20); int res = x[index * 4]; free(x); return res;}
int main(int argc, char **argv) {foo(argc + 19); printf("CHECK\n"); exit(0);}
EOF
	c++ -o test -m32 -fsanitize=address -pg test.cpp
	paxctl +a test
	atf_check -s not-exit:0 -o not-match:"CHECK\n" -e match:"heap-buffer-overflow" ./test
}

target_not_supported_body()
{
	atf_skip "Target is not supported"
}

atf_init_test_cases()
{
	test_target
	test $SUPPORT = 'n' && {
		atf_add_test_case target_not_supported
		return 0
	}

	atf_add_test_case heap_overflow
#	atf_add_test_case heap_overflow_profile
	atf_add_test_case heap_overflow_pic
	atf_add_test_case heap_overflow_pie
#	atf_add_test_case heap_overflow32
	# static option not supported
	# -static and -fsanitize=address can't be used together for compilation
	# (gcc version  5.4.0 and clang 7.1) tested on April 2nd 2018.
}
