#/bin/sh

OL=ethermem.log

mv -f $OL $OL.old

(
while true
do
	echo -n `date "+%F %X"` >> $OL;
	echo -n " " >> $OL
	ps axu | grep " etherape" | grep -v grep >> $OL;
	sleep 20;
done) 


