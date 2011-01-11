#!/bin/bash
# Simple script to talk to a vrtual serial port
#
# Required config:
#  [server]
#  DriverPath=server/drivers/
#  Driver=ax89063
#  [ax89063]
#  Device=./LCDd.tty

set -e

cd $(make getdir)

(sleep 1; server/LCDd -f -c ./LCDd.conf -r 4 "$@") &
socat PTY,link=./LCDd.tty,raw,echo=0,wait-slave -

