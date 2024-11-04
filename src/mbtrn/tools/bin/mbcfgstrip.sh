#!/usr/bin/env bash

# summary: show mbtrnpp config contents w/o comments

quiet='N'

if [ ${#} -ge 1 ]
then
 while  [ ${#} -ge 1 ]
 do
  cfg=$1
  if [ ${cfg} == "-q" ]
  then
   quiet='Y'
  else
   if [ $quiet == 'N' ]
   then
    echo "-- $cfg --"
   fi
   if [ -f $cfg ]
   then
    grep -e "[a-zA-Z0-9]" $cfg | grep -v -e "^[/#]"
    if [ $quiet == 'N' ]
    then
     echo
    fi
   fi
  fi
  shift
done
else
echo
echo " summary: show mbtrnpp config contents w/o comments"
echo " use: $(basename $0) [-q] <config>"
echo "  options:"
echo "    -q : quiet - no filename or trailing newline"
echo
echo " Examples:"
echo "  # show file (no separators)"
echo "    $(basename $0) -q foo.cfg"
echo
echo "  # show multiple files with separators"
echo "    $(basename $0) foo/bar/*.cfg"
echo

echo
fi
