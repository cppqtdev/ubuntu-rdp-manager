#!/bin/sh
# Generates a THROWAWAY test CA and an xrdp server certificate for the acceptance-test container.
# Usage: make-test-pki.sh <out-dir> <server-hostname>
set -eu
OUT="$1"; HOST="$2"
mkdir -p "$OUT"; cd "$OUT"
openssl req -x509 -newkey rsa:3072 -nodes -days 30 -subj "/CN=URDM Test CA" \
    -keyout ca.key -out ca.crt 2>/dev/null
openssl req -newkey rsa:3072 -nodes -subj "/CN=$HOST" -keyout server.key -out server.csr 2>/dev/null
printf "subjectAltName=DNS:%s\nextendedKeyUsage=serverAuth\nkeyUsage=digitalSignature,keyEncipherment\n" "$HOST" > ext.cnf
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial -days 30 \
    -extfile ext.cnf -out server.crt 2>/dev/null
rm -f server.csr ext.cnf ca.srl
echo "PKI written to $OUT"
