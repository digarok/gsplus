#!/bin/sh
git clone https://github.com/andreyvit/yoursway-create-dmg.git
cd yoursway-create-dmg

test -f GSplus-Install.dmg && rm GSplus-Install.dmg
./create-dmg \
  --volname "GSplus" \
  --volicon "../assets/gsp-dmg-icons.icns" \
  --background "../assets/gsback.png" \
  --window-pos 200 120 \
  --window-size 710 600 \
  --icon-size 64 \
  --icon GSplus.app 250 210 \
  --hide-extension GSplus.app \
  --app-drop-link 440 210 \
  --icon gsplusmanual.pdf  350 350 \
  --icon COPYRIGHT.txt  400 350 \
  --icon LICENSE.txt  470 350 \
  GSplus-Install.dmg \
  ../gsplus-osx/
cp GSplus-Install.dmg ..
