# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [0.1.0] - 2026-04-24

Initial snapshot.

### Added
- `malloc`, `free`, `calloc`, `realloc`.
- Single-list block management with `sbrk`.
- First-fit search, split on allocation.
- Tail-free shrinks the heap back to the kernel.
- Global mutex for thread safety.

## [0.2.0] - 2026-05-04

### Added
- `mymalloc.h` public API header with include guard.
- Boundary tags: 
    + `Footer` struct stored in free block data region for O(1) backward traversal.
    + `is_prev_allocated` field in block header for tracking previous block state.
    + `write_footer`, `get_prev_footer`, `get_prev_header(header, size)`, `get_next_header` helpers using pointer arithmetic.
- Coalescing:
    + Forward coalescing merges consecutive free blocks ahead on `free()`.
    + Backward coalescing merges with preceding free block on `free()` via footer lookup.
- Heap boundary detection using `sbrk(0)` to replace linked list termination.

### Changed
- Replaced `MemoryManager` struct and `next` pointer with arithmetic-based block traversal.
- `backward_coalescing` now runs in O(1) instead of O(n).
- `split_free_block` writes footer for remainder block and updates `is_prev_allocated` on neighbors.
- `free()` tail-shrink loop uses boundary tags instead of list walk for backward navigation.

### Fixed
- Undeclared `remove_size` variable in `free()`.
- `realloc(NULL, size)` now delegates to `malloc(size)`; `realloc(ptr, 0)` returns `NULL`.

