#!/bin/sh

DB=dai_main
MUN=root
MUP=$1

#if [ 0 -eq 1 ]; then
#	mysql --silent --skip-column-names -u $MUN -p$MUP $DB -e 'show tables' | awk '{print "drop table " $1 ";"}' | #mysql -u $MUN -p$MUP $DB
#	echo 1
#else
#	RP=
#	mysql -u root -p$RP -e "DROP DATABASE $DB;"
#	mysql -u root -p$RP -e "CREATE DATABASE $DB;"
#	mysql -u root -p$RP mysql -e "GRANT ALL PRIVILEGES ON *.* TO ${MUN}@localhost IDENTIFIED BY '$MUP' WITH GRANT OPTION;"
#	echo 0
#fi

mysql -u $MUN -p$MUP -e "SOURCE ${DB}.sql;"
mysql -u $MUN -p$MUP $DB -e "SOURCE data.sql;"

exit 0

