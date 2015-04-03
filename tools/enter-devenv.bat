@echo off
REM Copyright 2014 the Neutrino authors (see AUTHORS).
REM Licensed under the Apache License, Version 2.0 (see LICENSE).

REM Sets up a build environment. This file is copied among all the repos that
REM use mkmk, the authoritative version lives in mkmk/tools. Don't change this
REM directly.

REM Check that virtualenv is installed.
where /Q virtualenv
if ERRORLEVEL 1 (
  echo You need virutalenv to create a devenv.
  exit /b 1
)

REM Create the devenv virtualenv if it doesn't already exist.
if not exist devenv (
  echo Creating virtualenv devenv.
  virtualenv devenv
  if ERRORLEVEL 1 (
    echo Failed to create virtualenv.
    exit /b 1
  )
)

call devenv\Scripts\activate
if ERRORLEVEL 1 (
  echo Failed to enter devenv.
  exit /b 1
)

where /Q mkmk
if ERRORLEVEL 1 (
  REM There is no mkmk installed so try to install it.
  if not exist mkmk (
    REM We have to check out the mkmk github project.
    git clone https://github.com/tundra/mkmk.git mkmk
    if ERRORLEVEL 1 (
      echo Git cloning failed.
      exit /b 1
    )
  )
  REM Run the mkmk installer (develop really).
  cd mkmk
  python setup.py develop
  cd ..
)
