#!/bin/sh

STUID=b98902060

mkdir $STUID && \
	cp *.[ch] Makefile *.pdf $STUID && \
	tar zcvf $STUID.tar.gz $STUID

