@echo off
REM Copyright 2014 the Neutrino authors (see AUTHORS).
REM Licensed under the Apache License, Version 2.0 (see LICENSE).

REM Sets up a build environment. This file is copied among all the repos that
REM use mkmk, the authoritative version lives in mkmk/tools. Don't change this
REM directly.
REM
REM Unlike the linux version this one installs mkmk into the global environment.
REM That's because 1) windows doesn't require a password to do this (unlike
REM linux) and 2) seriously, virtualenv on windows?

where /Q mkmk
if ERRORLEVEL 1 (
  REM There is no mkmk installed so try to install it.
  if not exist mkmk (
    REM We have to check out the mkmk github project.
    git clone https://github.com/goto-10/mkmk.git mkmk
  )
  REM Run the mkmk installer (develop really).
  cd mkmk
  python setup.py develop
  cd ..
)
