#!/bin/bash

# Generate a list of slave IP addresses.
# For Docker, they are presumed to start at 172.17.0.3

# Get directory of script file
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

# Define SLAVELIST relative to script location
SLAVELIST="$DIR/../distributed-system/SLAVELIST"

# Delete slavelist if it exists
[ -e $SLAVELIST ] && rm $SLAVELIST

if [[ $1 -gt 65533 ]]; then
	echo "ERROR: Input of $1 will create an IP overflow. Max value is 65533."
	exit 1
fi

# Will break for input over approx 256*256
for (( i = 3; i < $1 + 3; i++ )); do
	echo "172.17.$(($i / 256)).$(($i % 256))" >> $SLAVELIST
done
