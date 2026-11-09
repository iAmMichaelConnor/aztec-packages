# Some possible improvements

Notes for self.

## Prover

- `commit_sparse` could avoid going through all the rows if we could provide it the set of rows where non-zero elements exist... and the trace object knows that! However, the improvement might not be amazing if `commit_sparse` already only considers the size of the polynomial (which it should) unless the column is mostly 0s and a non-zero elem at the end. Benchmarking should be done before trying this optimization.
- Along the same line of committing, we now use `commit_sparse` which has both performance (likely not much) and memory (might be up to 2x) overhead over just using `commit`. Given that we now use tight polynomials, it might make sense to use just `commit` UNLESS, again, the column is mostly 0s and a non-zero elem at the end. HOWEVER, we could cheaply decide this on the fly using the trace object which can keep the number of non-zero elems, and we use `commit_sparse` if below certain % threshold.

## Experimental memory optimizations

`bb-avm avm_prove --avm-memory-optimizations` enables both first-fold allocation
without zero initialization (only when the fold overwrites the allocation) and
direct PCS accumulation without per-CPU polynomial accumulators. Both default off.
The equivalent environment setting is `BB_AVM_MEMORY_OPTIMIZATIONS=1`; use `0` to
disable it. The command-line flag takes precedence over the environment setting.

C++ callers can select the same group with `AvmAPI(true)`, `AvmProvingHelper(true)`,
or the final boolean argument to `AvmProver`. The native file and byte proving APIs
also accept this optional boolean. Configuration belongs to each proving instance;
no process-global setting is changed. Circuit checking and verification are unaffected.

The prover fuzzer runs the baseline and optimized group on the same proving inputs,
verifies both proofs, and compares them. The first-fold tests cover both modes with
empty, shifted, odd-sized and sparse backing ranges. Trace generation and the shared
thread pool retain their existing implementations.
