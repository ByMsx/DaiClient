#!/bin/sh
## http://www.akadia.com/services/ssh_test_certificate.html

CER_NAME=dtls

openssl genrsa -des3 -out ${CER_NAME}.key 1024

openssl req -new -key ${CER_NAME}.key -out ${CER_NAME}.csr

cp ${CER_NAME}.key ${CER_NAME}.key.org
openssl rsa -in ${CER_NAME}.key.org -out ${CER_NAME}.key

openssl x509 -req -days 365 -in ${CER_NAME}.csr -signkey ${CER_NAME}.key -out ${CER_NAME}.crt

exit 0