# bcache

BCache is a simple Nix binary cache server using HTTP, with the features:

- signing of store paths
- support for bzip2, xz, lzma or no compression
- low resource usage (14MB with no compression)
