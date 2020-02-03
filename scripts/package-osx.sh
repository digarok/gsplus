#!/bin/sh
# run me from `build` subdirectory

# Create package directory and put any remaining files in place
PDIR=package-osx
rm -rf $PDIR  #start empty
mkdir $PDIR
cp -r ../build/src/GSplus.app $PDIR

DDIR=$PDIR/GSplus.app

mkdir -p $PDIR/license
cp ../LICENSE.txt $PDIR/license
cp ../COPYRIGHT.txt $PDIR/license
cp ../doc/gsplusmanual.pdf $PDIR
cp ../doc/README.txt $PDIR

# Bundle dynamic libraries
dylibbundler -od -b -x $DDIR/Contents/MacOS/gsplus -d $DDIR/Contents/libs/





# taken out DMG CI/CD for now as it requires keychain/UI interaction :( 
#exit

# Make DMG
git clone https://github.com/digarok/create-dmg.git
cd create-dmg

test -f GSplus-Install.dmg && rm GSplus-Install.dmg
./create-dmg \
  --volname "GSplus" \
  --volicon "../../assets/gsp-dmg-icons.icns" \
  --background "../../assets/gsback.png" \
  --window-pos 200 120 \
  --window-size 710 600 \
  --icon-size 64 \
  --icon GSplus.app 250 210 \
  --hide-extension GSplus.app \
  --app-drop-link 440 210 \
  --icon README.txt  225 350 \
  --icon gsplusmanual.pdf  350 350 \
  --icon license  470 350 \
  --skip-jenkins \
  GSplus-Install.dmg \
  ../package-osx/
mv GSplus-Install.dmg ..
