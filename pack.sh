#!/bin/sh

STUID=b98902060

mkdir $STUID && \
	cp *.[ch] Makefile Report *.png $STUID && \
	tar zcvf $STUID.tar.gz $STUID

