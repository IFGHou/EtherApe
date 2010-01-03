#/bin/sh

NL=0.8.2.log
OL=0.8.3.log

mv -f $NL $NL.old
mv -f $OL $OL.old

(
while true
do
	echo -n `date "+%F %X"` >> $NL;
	echo -n " " >> $NL
	ps axu | grep " etherape" | grep -v grep >> $NL;
	sleep 20;
done) &

(
while true
do
	echo -n `date "+%F %X"` >> $OL;
	echo -n " " >> $OL
	ps axu | grep "/etherape" | grep -v grep >> $OL;
	sleep 20;
done) &


