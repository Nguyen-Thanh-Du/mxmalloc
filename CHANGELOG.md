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
