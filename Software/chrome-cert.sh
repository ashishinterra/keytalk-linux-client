#!/bin/bash

certutil -d sql:$HOME/.pki/nssdb -A -t TCP,TCP,TCP -n $1 -i $2

echo ""
echo "To view all the certificates run 'certutil -d sql:$HOME/.pki/nssdb -L'"
