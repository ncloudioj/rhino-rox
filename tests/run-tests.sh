#!/bin/sh

rr=rhino-rox

if [[ ! -d venv ]]; then
    virtualenv --python=python2.7 --no-site-packages venv
    source venv/bin/activate
    pip install -r requirements.txt
fi

echo ""
echo "Starting server..."
cd ../src && ./${rr} > /dev/null 2>&1 &

echo "Running tests..."
pidfile=$(cd ../src && sed -n 's/^pidfile = \(.*\)$/\1/p' rhino-rox.ini)
cd ../tests && source venv/bin/activate && nosetests && deactivate

echo "Shutting down server..."
if [[ ! -z ${pidfile} ]]; then
	kill -TERM $(cat ${pidfile})
else
	pkill -TERM ${rr}
fi
