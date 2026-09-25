# sshnotif

> I like dev options but feels kind of weird to have ssh all over the place

## About

Sshnotif will send your phone a notification every time someone SSHs into it.

## Building

You need `sfosbuild` - install it with `go install github.com/mrcyjanek/sfosbuild`

```bash
$ sfosbuild 5.1.0.11 all . # build everything
$ sfosbuild 5.1.0.11 aarch64 . # build specific target
$ sfosbuild deploy defaultuser@192.168.1.177 . # to install it on target device
```