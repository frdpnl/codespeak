#!/bin/sh

TDIR="tests"
for t in $(ls $TDIR/t*); do
	OUT="$TDIR/out-$(basename $t)"
	REF="$TDIR/ref-$(basename $t)"
	./a.out <$t >$OUT
	echo -n "test $t "
	diff $OUT $REF
	if [ $? == 0 ]; then 
		echo "ok"
	else 
		echo "FAILED"
	fi
done

