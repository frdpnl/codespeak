#!/bin/bash

TDIR="tests"
for t in $(ls $TDIR/t*); do
	OUT="$TDIR/out-$(basename $t)"
	REF="$TDIR/ref-$(basename $t)"
	./a.out debug <$t >$OUT
	grep -v "^#" $OUT > $OUT.r
	grep -v "^#" $REF > $REF.r
	diff $OUT.r $REF.r
	if [ $? != 0 ]; then 
		echo "$t FAILED"
	fi
done

