#!/usr/bin/env bash

# Script which prints out the list of the ciphers supported by the server to stdout

# Adapted from
# http://superuser.com/questions/109213/is-there-a-tool-that-can-test-what-ssl-tls-cipher-suites-a-particular-website-of

function usage()
{
    echo "Usage: $0 IP_ADDRESS:PORT" 1>&2
    exit 2;
}

# OpenSSL requires the  IP address AND port number.
if [ $# -ne 1 ]; then
    usage
fi

SERVER="$1"
ciphers=$(openssl ciphers 'ALL:eNULL' | sed -e 's/:/ /g')

supported=()
for cipher in ${ciphers[@]}
do
    result=$(echo -n | openssl s_client -cipher "$cipher" -connect $SERVER 2>&1 | grep "connect\: Connection refused")
    if [[ ! -z $result ]] ; then
        echo CONNECTION REFUSED  1>&2
    else
        result=$(echo -n | openssl s_client -cipher "$cipher" -connect $SERVER 2>&1 | grep "\:error\:")
        if [[ -z $result ]] ; then
            echo "SUPPORTED: "$cipher
        fi
    fi
    # delay makes sense when testing against the real servers
    # sleep 0.1
done

