# mymalloc

A dynamic memory allocator written in C, built from scratch for learning.
Inspired by [dlmalloc](https://gee.cs.oswego.edu/pub/misc/malloc.c).

## Status

Early stage. Works for basic `malloc` / `free` / `calloc` / `realloc` flows.
Not production-ready.

## Build

```sh
cc -Wall -Wextra -o mymalloc mymalloc.c -lpthread
```

## License

MIT
