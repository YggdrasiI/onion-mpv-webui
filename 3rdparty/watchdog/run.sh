#!/bin/env bash

# Run both watchdogs parallel

onINT() {
kill -INT "$command1PID"
kill -INT "$command2PID"
exit
}

echo "Starting watchdogs"

trap "onINT" SIGINT
make -f Makefile_js run &
command1PID="$!"
make -f Makefile_css run  &
command2PID="$!"

# Wait on Ctrl+C
sleep infinity

echo "Exiting watchdogs"
