#!/bin/sh

set -v -e

sudo apt-get install python-pip

sudo pip install virtualenv

virtualenv neudev
