#!/bin/bash

#######################################################################################################################################################
# The script periodically takes a camera shot, gets a client cert with KeyTalk client and uses this cert to upload the image to the FTPS server
#######################################################################################################################################################

# rccd is taken from http://keytalk.com/demo.rccd, add m2m service manually

set -o errexit;
set -o nounset;

KEYTALK_PROVIDER=KeyTalk_Demo
KEYTALK_SERVICE=m2m
KEYTALK_USER=Raspberry1
KEYTALK_PASSWD='change1!'

FTP_SERVER_ADDRESS="ftp://m2m.keytalk.com:21/pics/"
FTP_SERVER_CREDS="ftpuser:gehE1m4ktDEMO"

echo "[$(date)] Starting KeyTalk DEMO..."

while :
do
    IMAGE_NAME=/tmp/cam_$(date +%Y%m%d%H%M%S).jpg

    echo "[$(date)] Taking a shot..."
    if raspistill --timeout 1000 --width 1296 --height 972 --output ${IMAGE_NAME} ; then

        echo "[$(date)] Getting a certificate..."
        if /usr/local/bin/keytalk/ktclient --provider ${KEYTALK_PROVIDER} --service ${KEYTALK_SERVICE} --user ${KEYTALK_USER} --password ${KEYTALK_PASSWD} ; then

            CERT_FILE_NAME=$(ls -dt ~/.keytalk/keystore/*.pem | head -1)

             echo "[$(date)] Uploading the image..."
             if ! curl --silent --show-error --insecure --cert ${CERT_FILE_NAME} --user ${FTP_SERVER_CREDS} --ftp-ssl-reqd ${FTP_SERVER_ADDRESS} --upload-file ${IMAGE_NAME} ; then
                echo "[$(date)] ERROR UPLOADING IMAGE"
             fi
        else
            echo "[$(date)] ERROR GETTING A CERTIFICATE"
        fi

        # cleanup
        rm -f ${IMAGE_NAME}
        echo
    else
        echo "[$(date)] ERROR TAKING A SHOT"
    fi

    sleep 0.5
done