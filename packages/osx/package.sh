#!/bin/sh

set -e

if [ ! -r jre_1.8.0_151.tar.gz ]; then
   echo jre_1.8.0_151.tar.gz is missing
   exit 1
fi

if [ -z $1 ]; then
   version=`grep AC_INIT ../../configure.ac|cut -d'[' -f3|cut -d']' -f1`
else
   version=$1
fi

rm -rf dist/* nxmc-${version}.dmg
~/Development/yoursway-eclipse-osx-repackager/EclipseOSXRepackager out/eclipse dist/NetXMS\ Console.app

cat Info.plist | sed "s,@version@,${version},g" > "dist/NetXMS Console.app/Contents/Info.plist"
patch -p0 < nxmc.ini.diff
tar zxf jre_1.8.0_151.tar.gz -C dist/NetXMS\ Console.app/Contents/MacOS/
codesign --deep --force -s 'Developer ID Application: Raden Solutions, SIA (KFFD69X4L6)' -v 'dist/NetXMS Console.app'
create-dmg --volname "NetXMS ${version}" --volicon ~/Development/netxms/netxms/packages/osx/dmg-icon.icns --window-size 500 320 --background ~/Development/netxms/netxms/packages/osx/dmg-background.png --icon "NetXMS Console.app" 114 132 --app-drop-link 399 132 nxmc-${version}.dmg dist
codesign -s 'Developer ID Application: Raden Solutions, SIA (KFFD69X4L6)' nxmc-${version}.dmg
