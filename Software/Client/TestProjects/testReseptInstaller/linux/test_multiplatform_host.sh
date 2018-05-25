#!/bin/bash

# Script executed by host buildserver which invokes client tests on multiple platforms

set -e
set -u

SUCCEEDED_TESTS=0
FAILED_TESTS=0

# dictionary { image name => build-server-address }
TEST_IMAGES=\
"debian-jessie-keytalk-test:192.168.33.109\
 ubuntu-16.04-keytalk-test:192.168.33.109\
 debian-stretch-keytalk-test:192.168.33.110"
LOG_DIR=/var/log/keytalk

rm -rf ${LOG_DIR}
mkdir -p ${LOG_DIR}

function fmt_time()
{
    local total_seconds="$1"
    ((hours=${total_seconds}/3600))
    ((minutes=(${total_seconds}%3600)/60))
    ((seconds=${total_seconds}%60))
    printf "%02d:%02d:%02d\n" $hours $minutes $seconds
}

function cleanup()
{
    # Cleanup all stopped containers and untagged docker images
    # since they invade disk space over time
    if [ x"$(docker ps -a -q)"  != x"" ]; then
        docker rm $(docker ps -a -q)
    fi
    if [ x"$(docker images | grep "^<none>")" != x"" ]; then
        docker rmi $(docker images | grep "^<none>" | awk '{print $3}')
    fi
}

function elapsed_time()
{
    local start_time="$1"
    local elapsed_seconds=$(expr `date +%s` - $start_time)
    fmt_time ${elapsed_seconds}
}

function test_guest()
{
    # populate container with the buildserver ssh keys so it can fetch installers from the buildserver without password,
    # share the test scripts,
    # specify where to write logs
    # and fire up the tests in the container

    local image_name="$1"
    local bsvr_address="$2"
    local start_time=`date +%s`
    echo "Start testing ${image_name} using build server from ${bsvr_address}"

    if docker run \
       --volume ~/.ssh:/root/.ssh:ro \
       --volume `pwd`:/test/bootstrap:ro \
       --volume ${LOG_DIR}/${image_name}/:/test/log:rw \
       --env BSVR_ADDRESS=${bsvr_address} \
       --workdir=/test \
       --add-host=demo.keytalkdemo.com:192.168.33.111 \
       ${image_name}
    then
        local test_status="SUCCEEDED"
        SUCCEEDED_TESTS=$((SUCCEEDED_TESTS+1))
    else
        local test_status="FAILED"
        FAILED_TESTS=$((FAILED_TESTS+1))
    fi
    local test_time=$(elapsed_time "${start_time}")
    echo "TEST ${image_name} ${test_status} (${test_time})"
}

START_TIME=`date +%s`
cleanup
for image in ${TEST_IMAGES} ; do
    IMAGE_NAME=$(echo ${image} | cut -d ':' -f 1)
    BSVR_ADDR=$(echo ${image} | cut -d ':' -f 2)
    test_guest ${IMAGE_NAME} ${BSVR_ADDR}
done
cleanup
TOTAL_TEST_TIME=$(elapsed_time "${START_TIME}")
TOTAL_TESTS_RUN=$((SUCCEEDED_TESTS+FAILED_TESTS))
echo "TOTAL: tested ${TOTAL_TESTS_RUN} images: ${SUCCEEDED_TESTS} succeeded, ${FAILED_TESTS} failed (${TOTAL_TEST_TIME})"
if [ "${FAILED_TESTS}" -eq "0" ]; then
    exit 0
else
    exit 1
fi
