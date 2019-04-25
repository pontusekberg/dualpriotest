# dualpriotest

This program verifies (by exhaustive checking) the three counterexamples to dual priority scheduling
conjectures given in the paper entitled [Dual Priority Scheduling is Not
Optimal](http://user.it.uu.se/~ponek616/files/preprints/dualprio-preprint.pdf),
published at the Euromicro Conference on Real-Time Systems (ECRTS), 2019.

As this program serves the purpose of a proof, much of it is written in a naive
style in order to be as easy as possible for a human reader to verify correct,
even when this breaks usual good programming practices.  For example, as all
the counterexamples have four tasks, this program is hardcoded for task sets
with exactly four tasks. Combinations and permutations are generated with deep
nested loops instead of with more elegant (but more complicated) methods. To
avoid any extra complexity, this program is also single-threaded, despite the
inherent parallelism that is possible when testing vast numbers of
configurations.

All counterexamples have been verified with the below compilers and CPUs.

*Tested compilers:* GCC 8.2.1/5.5.0/4.9.2, and Clang/LLVM 7.0.1

*Tested CPUs:* AMD Ryzen 7 1700X, AMD Opteron 2220 SE, and Intel Xeon E5520

## Usage

This program should compile on any standards-compliant C99 compiler. High
compiler optimization settings (e.g., -O3 for gcc or clang) are recommended.
Just type `make` to compile using the provided Makefile.

	Usage: dualpriotest TEST_NUM

	where TEST_NUM is 1, 2, or 3.

	Test 1: Show the suboptimality of dual priority scheduling.
        Counterexample 8 in the paper (very, very slow).

	Test 2: Show the suboptimality of RM ordering of phase 1 priorities
        Counterexample 9 in the paper (very slow).

	Test 3: Show the suboptimality of FDMS phase change points
        Counterexample 10 in the paper (fast).
