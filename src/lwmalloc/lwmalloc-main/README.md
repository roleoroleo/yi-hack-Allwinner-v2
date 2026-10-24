# LWMalloc: A Lightweight Dynamic Memory Allocator for Resource-Constrained Environments
LWMalloc is a dynamic memory allocator for resource-constrained systems. It can be injected at runtime to replace malloc/calloc/realloc/free via LD_PRELOAD.

## FILES
----------------------------------------------------------------------
- lwmalloc.c : allocator implementation
- Makefile   : build script (produces a shared object)

## BUILD
----------------------------------------------------------------------
```bash
make
# Output: lwmalloc.so
```

## USE AT RUNTIME (LD_PRELOAD)
----------------------------------------------------------------------
Per-process:
LD_PRELOAD=/absolute/or/relative/path/to/lwmalloc.so your_program [args]

Session-wide:
export LD_PRELOAD=/absolute/path/to/lwmalloc.so
your_program [args]
unset LD_PRELOAD

Tip: Prefer an absolute path for LD_PRELOAD.

## QUICK TEST
----------------------------------------------------------------------
A simple test program (test.c) is included in this repository.

1) Build the test:
```bash
gcc -O2 -o test test.c
```

3) Run the test with LWMalloc preloaded:
```bash
LD_PRELOAD=./lwmalloc.so ./test
```

## NOTES & LIMITATIONS
----------------------------------------------------------------------
- Static binaries and some setuid programs may ignore LD_PRELOAD due to loader/security policies.
- Do not preload multiple user-space allocators at the same time.
- If behavior is unexpected, unset LD_PRELOAD and re-run to compare against the system allocator.
