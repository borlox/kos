# Version updater for kOS
FILE=share/include/kos/version.h
VER="kos_version = \"`git describe`\";"
DATE="kos_builddate = \"`date +"%T - %d.%m.%y"`\";"

NAME="kos_buildname = \"$KOS_BUILDNAME\";"

if [ ! -e $FILE ]
then
	cat > $FILE << STOP 
#ifndef KOS_VERSION_H
#define KOS_VERSION_H

__attribute__((unused)) static const char *kos_version = "version";
__attribute__((unused)) static const char *kos_buildname = "name";
__attribute__((unused)) static const char *kos_builddate = "date";

#endif /*KOS_VERSION_H*/	
STOP
fi

mv $FILE $FILE.old
sed "s/kos_version = \"[^\"]*\"\;/$VER/g;s/kos_builddate = \"[^\"]*\"\;/$DATE/g;s/kos_buildname = \"[^\"]*\"\;/$NAME/g" $FILE.old > $FILE
rm $FILE.old
