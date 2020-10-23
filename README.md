# SRS

## Introduction
Forked from ossrs/srs  

Main branch should be `3.0release` which is more stable than `develop` or `4.0release`

Applied patches:
- https://github.com/ossrs/srs/pull/1753
- https://github.com/ossrs/srs/pull/1754

## Compiling
```sh
cd trunk
./configure --use-sys-ssl --with-utest --with-ssl --with-librtmp
make -j12
```
- TODO: doesn't work with g++ 10.x

## Building
See mycujoo/srs-docker