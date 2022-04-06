# bcache

Nix binary cache server

## WARNING

This program is __EXPERIMENTAL__ and everything can change.

## Description

Similiar to [nix-serve], but rewritten from Perl to C++ for less memory usage.

[nix-serve]: https://github.com/edolstra/nix-serve

## Build
```
nix-shell
make
```

## Run
```
bcache --compression gzip --socket /path/to/socket
```

## Licence

LGPLv2.1
