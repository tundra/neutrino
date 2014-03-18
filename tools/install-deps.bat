REM Installs build dependencies.

cd deps\plankton\src\python
python setup.py develop

cd ..\..\..\mkmk
python setup.py develop

cd ..\..
