#!/bin/sh

[ -z "${DB}" ] && DB=${1}
[ -z "${UU}" ] && UU=${2}
[ -z "${PP}" ] && PP=${3}

if [ -z "${UU}" ] || [ -z "${DB}" ]; then
  echo "Usage ${0} database user [password]"
  exit 1
fi

if [ -z "${PP}" ]; then
  echo -n "Please enter password for user ${UU}: "
  read line
  PP=${line}
fi

#UU=DaiUser
#DB=dai_main

mysqldump -u ${UU} -p${PP} --complete-insert --skip-dump-date --compact --hex-blob --no-create-info --no-create-db --ignore-table=${DB}.gh_logs --ignore-table=${DB}.gh_eventlog -B ${DB} > data.sql
mysqldump -u ${UU} -p${PP} --skip-dump-date --add-drop-database --add-drop-table --no-data -a -B ${DB} > structure.sql

echo "
-- CREATE USER IF NOT EXISTS '${UU}'@'localhost' IDENTIFIED BY '${PP}';
GRANT ALL ON ${DB}.* TO '${UU}'@'localhost' IDENTIFIED BY '${PP}';
GRANT ALL PRIVILEGES ON ${DB} . * TO '${UU}'@'localhost';
" >> ${DB}.sql

exit 0

