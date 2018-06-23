for ((i=0; i<=1200; i+=10))
do
    /usr/bin/time -ao time_cent.txt -f "$i\t%E" ./master_cent "$i"
done
