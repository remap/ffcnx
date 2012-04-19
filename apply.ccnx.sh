#! /bin/bash

PATCH_FILES=$(find . -name '*.ccnx.patch')
for FILE in $PATCH_FILES; do
echo $FILE
  patch -p1 < $FILE
done
