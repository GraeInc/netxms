#!/bin/sh

set -e

cd build
./updatetag.pl
cd ..
cp build/netxms-build-tag.properties src/java-common/netxms-base/src/main/resources/

VERSION=`cat build/netxms-build-tag.properties | grep NETXMS_VERSION | cut -d = -f 2`

trap '
   mvn -f src/pom.xml versions:revert -DprocessAllModules=true
   mvn -f src/client/nxmc/java/pom.xml versions:revert
' EXIT

mvn -f src/pom.xml versions:set -DnewVersion=$VERSION -DprocessAllModules=true
mvn -f src/client/nxmc/java/pom.xml versions:set -DnewVersion=$VERSION

mvn -f src/pom.xml install -Dmaven.test.skip=true

if [ "$1" = "all" ]; then   
   #all management clients build
   WORKIND_DIR='src/client/nxmc/java'
   mvn -f $WORKIND_DIR -Dmaven.test.skip=true -Pdesktop -Pstandalone clean package 
   cp $WORKIND_DIR/target/nxmc-${VERSION}-*.jar $WORKIND_DIR/
   mvn -f $WORKIND_DIR -Dmaven.test.skip=true -Pdesktop -Pstandalone -P\!linux-x86_64 -Pwindows clean package
   cp $WORKIND_DIR/target/nxmc-${VERSION}-*.jar $WORKIND_DIR/
   mvn -f $WORKIND_DIR -Dmaven.test.skip=true -Pdesktop -Pstandalone -P\!linux-x86_64 -Plinux-aarch64 clean package
   cp $WORKIND_DIR/target/nxmc-${VERSION}-*.jar $WORKIND_DIR/
   mvn -f $WORKIND_DIR -Dmaven.test.skip=true -Pdesktop -Pstandalone -P\!linux-x86_64 -Pmacos-x86_64 clean package
   cp $WORKIND_DIR/target/nxmc-${VERSION}-*.jar $WORKIND_DIR/
   mvn -f $WORKIND_DIR -Dmaven.test.skip=true -Pdesktop -Pstandalone -P\!linux-x86_64 -Pmacos-aarch64 clean package
   cp $WORKIND_DIR/target/nxmc-${VERSION}-*.jar $WORKIND_DIR/
   mvn -f $WORKIND_DIR -Dmaven.test.skip=true -Dnetxms.build.disablePlatformProfile=true -Pweb clean package
   cp $WORKIND_DIR/target/nxmc-${VERSION}*.war $WORKIND_DIR/
   #build REST API
   mvn -f src/client/nxapisrv/java -Dmaven.test.skip=true clean package   
   #build nxshell
   mvn -f src/client/nxshell/java -Dmaven.test.skip=true -Pstandalone clean package 
else
   mvn -f src/client/nxmc/java/pom.xml install -Pdesktop
fi

