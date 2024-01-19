#!/bin/sh
/usr/lib/amx/wld/debugInfo.sh
for FILE in $(ls -1 /usr/lib/amx/wld/modules/*/debugInfo.sh 2> /dev/null); do
   $FILE
done
