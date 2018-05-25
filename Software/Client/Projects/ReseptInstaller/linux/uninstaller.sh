#!/bin/bash

# parse the list of customized users into "user1" "user2"...
function parse_customized_users()
{
    if [ ! -r "/etc/keytalk/resept.ini" ]
    then
        echo ""
        return;
    fi

    exec < "/etc/keytalk/resept.ini"

    while read keyval; do
        if echo "${keyval}" | egrep -q '^[[:space:]]*CustomizedUsers[[:space:]]*=[[:space:]]*\[.*?\];$'
        then
            users=$(echo "${keyval}" | awk -F '=' '{print $2}')

            # For now we have something like [ "user1", "user2" ];
            # We need to parse it into user1 user2

            # strip leading and trailing whitespace
            users=$(echo ${users})
            # strip trailing ;
            users="${users%%;}"
            # strip brackets
            users=${users##\[}
            users=${users%%\]}
            # turn comma delimiter into space (this is safe since comma is not allowed in Unix usernames)
            users=$(echo ${users} | tr ',' ' ')
            # remove quotes around usernames (this is safe since quotes are not allowed in Unix usernames)
            users=$(echo ${users} | tr -d '"')

            echo ${users}
            return
        fi
    done

    echo ""
}

# remove KeyTalk user settings for all customized KeyTalk users
function remove_keytalk_user_settings()
{
    for user in $(parse_customized_users) ; do
        echo " Cleaning up KeyTalk customization for user '"${user}"'"
        if getent passwd ${user} > /dev/null 2>&1 ; then
            local homedir=$(getent passwd ${user} | cut -d: -f6)
            if [ -d ${homedir} ]; then
                local keytalk_user_profile_dir=${homedir}"/.keytalk"
                if [ -d ${keytalk_user_profile_dir} ]; then
                    echo "  removing KeyTalk user settings for user '"${user}"' from "${keytalk_user_profile_dir}
                    if ! rm -rf ${keytalk_user_profile_dir} ; then
                        echo "  WARNING: cannot remove KeyTalk user settings for user '"${user}"' from "${keytalk_user_profile_dir}
                    fi
                fi
            fi
        fi
    done
}

function remove_keytalk_common_settings()
{
    rm -rf /usr/local/bin/keytalk/
    rm -rf /usr/local/lib/keytalk/
    rm -rf /etc/keytalk/
    rm -rf /usr/share/doc/keytalk/

    rm -f /etc/cron.d/keytalk

    rm -f /usr/local/share/ca-certificates/keytalk_*.crt
    update-ca-certificates --fresh > /dev/null 2>&1
}

function uninstall()
{
    echo "Uninstalling KeyTalk..."

    killall --quiet ktclient
    remove_keytalk_user_settings
    remove_keytalk_common_settings
}

#
# Entry point
#

uninstall
