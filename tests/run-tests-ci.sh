#!/bin/bash

rr=rhino-rox
pidfile=$(cd ../src && sed -n 's/^pidfile = \(.*\)$/\1/p' rhino-rox.ini)

echo ""
echo "Starting server..."
cd ../src && ./${rr} > /dev/null 2>&1 &

echo "Running tests..."
cd ../tests && nosetests

echo "Shutting down server..."
if [[ ! -z ${pidfile} ]]; then
    kill -TERM $(cat ${pidfile})
else
    pkill -TERM ${rr}
fi
