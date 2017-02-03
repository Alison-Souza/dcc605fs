#!/bin/sh
set -u

# clean.sh by Alison Souza

rm gcc.log img test1* test2* test3* test4* test5* test6* 2>/dev/null || true

echo "Arquivos apagados!"

exit 0
