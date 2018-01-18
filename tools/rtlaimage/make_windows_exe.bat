rem pip install wheel
rem pip install pyinstaller
pyinstaller -c --onedir --onefile -n rtlaimage rtlaimage.py
copy /b dist\rtlaimage.exe ..\..\USDK\component\soc\realtek\8195a\misc\iar_utility\common\tools\rtlaimage.exe
del /Q rtlaimage.spec
rm -rf dist build
