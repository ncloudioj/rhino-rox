#!/bin/sh

set -e

if [[ ! -d venv ]]; then
    virtualenv --python=python2.7 --no-site-packages venv
    source venv/bin/activate
    pip install -r requirements.txt
fi
