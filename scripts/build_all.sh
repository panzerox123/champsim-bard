#!/bin/sh

make clean && make configclean
./config.sh json/baseline_open.json
make -j8

make clean && make configclean
./config.sh json/baseline_close.json
make -j8

make clean && make configclean
./config.sh json/wlru_open.json
make -j8

make clean && make configclean
./config.sh json/wlru_close.json
make -j8
