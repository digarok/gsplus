DEXTRAS=gsplus-osx/
DDIR=gsplus-osx/GSplus.app
ADIR=assets
mkdir -p $DDIR/Contents/MacOS
mkdir -p $DDIR/Contents/Resources

cp gsplus $DDIR/Contents/MacOS
cp config.txt $DDIR/Contents/MacOS
cp $ADIR/Info.plist $DDIR/Contents
cp $ADIR/gsp-icons.icns $DDIR/Contents/Resources
dylibbundler -od -b -x $DDIR/Contents/MacOS/gsplus -d $DDIR/Contents/libs/

# files to include in dmg
cp doc/gsplusmanual.pdf $DEXTRAS
cp doc/gsplusmanual.txt $DEXTRAS
cp doc/README.txt $DEXTRAS
cp COPYING.txt $DEXTRAS

# packaging now in DMG script
