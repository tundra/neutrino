#!/bin/sh

set -e

if which mkmk > /dev/null; then
  echo "An mkmk command is already available."
  exit 0
fi

if [ ! -d mkmk ]; then
  echo "Trying to check out mkmk from github."
  git clone https://github.com/goto-10/mkmk.git mkmk
fi

echo "Trying to install python package."
cd mkmk
python setup.py develop
