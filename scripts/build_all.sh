#!/bin/sh

make clean && make configclean
./config.sh json/baseline.json
make -j8

make clean && make configclean
./config.sh json/wcache.json
make -j8
