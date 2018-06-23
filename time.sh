for ((i=0; i<=1200; i+=10))
do
    /usr/bin/time -ao time.txt -f "$i\t%E" ./master "$i"
done
