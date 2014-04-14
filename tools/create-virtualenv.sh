#!/bin/sh

set -v -e

NAME=neutrino-devel

sudo apt-get install python-pip

sudo pip install virtualenv

virtualenv $NAME

. $NAME/bin/activate
