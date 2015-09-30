#!/usr/bin/env bash
#
# This script will periodically check the amount of free memory on the host box
# and kill the pcfg_manager process when free memory gets low.
#
# Written by Saranga Komanduri
# Based loosely on http://unix.stackexchange.com/a/224559
#
PROCNAME='pcfg_manager'

while true; do

    var1=$( ps h -o pmem -C "$PROCNAME"  | awk '{print $1}' )
    var2=$( df -BM . | awk 'NR == 2 {print $4}' | sed 's/M$//' )


    # Kill process when it uses 50% of memory
    # Need to use awk to compare floats
    if (( $(echo "$var1 50.0" | awk '{n1=$1; n2=$2; if (n1<n2) printf "0"; else printf "1"}') )); then
        echo "Memory usage too high, killing process"
	    echo -e "\a"
        pkill -9 "$PROCNAME"
        break
    else
        echo "Free memory is $var1"
    fi

    # Kill process when free disk space is less than 40 GB
    if [ $var2 -lt 40000 ]; then
        echo "Disk usage too high, killing process"
        echo -e "\a"
        pkill -9 "$PROCNAME"
        break
    else
        echo "Free disk space is $var2 MB"
    fi

    sleep 1m

done
