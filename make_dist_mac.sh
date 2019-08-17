DEXTRAS=gsplus-osx/
DDIR=$DEXTRAS/GSplus.app
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
cp LICENSE.txt $DEXTRAS
cp COPYRIGHT.txt $DEXTRAS

# packaging now in DMG script
