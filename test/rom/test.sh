#!/bin/bash
export LC_CTYPE=C
export LANG=C
export LC_ALL=C

# Download the firmware
wget 'http://raymii.org/s/inc/downloads/es40-srmon/cl67srmrom.exe'

# Start AXPbox
if [[ -f ../../../build/axpbox ]]; then
  ../../../build/axpbox run &
  AXPBOX_PID=$!
else # Travis
  ../../build/axpbox run &
  AXPBOX_PID=$!
fi

# Wait for AXPbox to start
sleep 5

# Connect to terminal
nc -t 127.0.0.1 21000 | tee axp.log &
NETCAT_PID=$!

# Wait for the last line of log to become P00>>>
timeout=600
while true
do
  if [ $timeout -eq 0 ]
  then
    echo "waiting for SRM prompt timed out" >&2
    exit 1
  fi

  # print last line and remove null byte from it
  if [ "$(LC_ALL=C sed -n '$p' axp.log | LC_ALL=C sed 's/\x00//g')" == "P00>>>"  ]
  then
    echo
    break
  fi

  sleep 1
  timeout=$(($timeout - 1))
done

kill $NETCAT_PID
kill $AXPBOX_PID

echo -n -e '\033[1;31m'
# The CPU speed line is measured from host wall-clock performance since the
# ES40-Emu timing port, so it varies per host; exclude it from the comparison
# (after stripping the NUL padding the SRM console emits).
normalize() { LC_ALL=C sed 's/\x00//g' "$1" | LC_ALL=C sed 's/CPU [0-9] speed is.*//'; }
diff -c <(normalize axp_correct.log) <(normalize axp.log) \
    && echo -e '\033[1;32mdiff clean\033[0m'
result=$?
echo -n -e '\033[0m'

rm -f axp.log cl67* *.rom
exit $result
