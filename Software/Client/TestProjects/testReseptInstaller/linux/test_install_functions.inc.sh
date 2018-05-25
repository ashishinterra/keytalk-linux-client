#!/bin/bash

# The include file contains misc helper functions used by installation test scripts

DEFAULT_CONTENT_VERSION="2011032901"

INSTALLATION_FILES_REQUIRED="\
/usr/local/bin/keytalk/ktclient \
/usr/local/bin/keytalk/ktconfig \
/usr/local/bin/keytalk/ktconfupdater \
/usr/local/bin/keytalk/ktconfigtool \
/usr/local/bin/keytalk/ktprgen \
/usr/local/bin/keytalk/hwutils \
/usr/local/bin/keytalk/renew_apache_ssl_cert \
/usr/local/bin/keytalk/util.py \
/usr/local/bin/keytalk/apache_util.py \
/usr/local/bin/keytalk/uninstall_keytalk \
/usr/local/lib/keytalk/libtalogger.so \
/etc/keytalk/resept.ini \
/etc/keytalk/apache.ini \
/etc/keytalk/version \
/etc/keytalk/devstage \
/etc/keytalk/cr.conf \
/etc/cron.d/keytalk \
/usr/share/doc/keytalk/KeyTalk_LinuxClient_for_Apache.txt \
/usr/share/doc/keytalk/KeyTalk_LinuxClient_for_Apache.pdf"

INSTALLATION_DIRS_REQUIRED="\
/usr/local/bin/keytalk \
/usr/local/lib/keytalk \
/etc/keytalk \
/usr/share/doc/keytalk"

PR_FILES_REQUIRED_PATTERN="\
version \
devstage \
user\.ini \
resept\.ini \
apache\.ini \
etc_cron_d_keytalk \
apache_ports\.conf \
ktclient\.log \
ktconfig\.log \
ktconfupdater\.log \
apache_error\.log \
signing_ca_[0-9a-f]*\.pem \
comm_ca_[0-9a-f]*\.pem \
primary_ca_[0-9a-f]*\.pem"


# usage client_platform_file_suffix="$(get_client_platform_file_suffix)"
function get_client_platform_file_suffix()
{
    local distro_name=$(lsb_release --id --short)
    local distro_version_major=$(lsb_release --release --short | egrep -o [0-9]+ | sed -n '1p')
    local arch=$(uname -m)
    local test_platform="${distro_name}-${distro_version_major}-${arch}"
    if [ "${test_platform}" == "Debian-8-x86_64" -o "${test_platform}" == "Ubuntu-16-x86_64" ]; then
      echo "debian8_ubuntu16.04-x64"
      return 0
    elif [ "${test_platform}" == "Debian-9-x86_64" ]; then
      echo "debian9-x64"
      return 0
    else
      echo "ERROR: ${test_platform} platform is not supported" >&2
      return 1
    fi
}

#usage cleanup_keytalk_installation [logs-backup-dir]
function cleanup_keytalk_installation()
{
  if pgrep -x ktclient ; then
      echo "WARNING: terminating active ktclient processes"
      killall -v ktclient
  fi
  if pgrep -x ktconfig ; then
      echo "WARNING: terminating active ktconfig processes"
      killall -v ktconfig
  fi

  [ $# -eq 1 ] && copy_keytalk_logs_to "$1"

  rm -rf ~/.keytalk/
  rm -rf ${INSTALLATION_DIRS_REQUIRED}
  rm -f /etc/cron.d/keytalk
  rm -f /usr/local/share/ca-certificates/keytalk_*.crt
  # ignore if update-ca-certificates is not installed
  update-ca-certificates --fresh || true
}

# usage parse_rccd_props $rccd_file provider rccd_type content_version
# filename is settings.<ProviderName>.<rccd-type>[.<content-version>].rccd
function parse_rccd_props()
{
    local rccd1_path="$1"
    local filename=$(basename ${rccd1_path})
    eval "$2"=$(echo ${filename} | cut -d . -f 2)
    eval "$3"=$(echo ${filename} | cut -d . -f 3)
    local component4=$(echo ${filename} | cut -d . -f 4)
    local content_version=${DEFAULT_CONTENT_VERSION}
    if [ x"${component4}" != x"rccd" ]; then
        content_version=${component4}
    fi
    eval "$4"=${content_version}
}

# usage find_cert ${dir} ${cn}
function find_cert_with_cn()
{
    local dir="$1"
    local cn="$2"

    for cert in $(ls ${dir}) ; do
        if openssl x509 -subject -noout -in ${cert} | grep -q "CN\s*=\s*${cn}" ; then
            return 0
        fi
    done
    echo "ERROR: No certificate with CN ${cn} found under ${dir}" >&2
    return 1
}

function verify_installation()
{
    for f in ${INSTALLATION_FILES_REQUIRED} ; do
        if [ ! -f ${f} ] ; then
            echo "ERROR: Installation failed. ${f} not found" >&2
            return 1
        fi
    done
}

# usage; verify_customization ${rccd1_path} ${rccd1_provider} ${rccd1_type} [${rccd2_path} ${rccd2_provider} ${rccd2_type}]
function verify_customization()
{
    local rccd_path="$1"
    local rccd_provider="$2"
    local rccd_type="$3"

    local customization_files_required=$(_customization_files_required ${rccd_provider} ${rccd_type})
    for f in ${customization_files_required} ; do
        if [ ! -f ${f} ] ; then
            echo "ERROR: Customization with ${rccd_path} failed. ${f} not found" >&2
            return 1
        fi
    done

    if ! grep -q "\"${rccd_provider}\"" /etc/keytalk/resept.ini ; then
        echo "ERROR: Customization with ${rccd_path} failed. Provider ${rccd_provider} not found in /etc/keytalk/resept.ini" >&2
        return 1
    fi

    # Verify installed CAs (3 at least but there might also be extra CAs from 3rd party signer such as GlobalSign)
    local num
    num=$(ls /usr/local/share/ca-certificates/keytalk_*.crt | wc -l)
    if (( num < 3 )) ; then
        echo "ERROR: Installation failed. Invalid number of intermediate CAs installed. Actual: ${num}, expected at least 3" >&2
        return 1
    fi

    if ! find_cert_with_cn "/usr/local/share/ca-certificates/keytalk_*.crt" "KeyTalk Demo Signing CA" ; then
        return 1
    fi
    if ! find_cert_with_cn "/usr/local/share/ca-certificates/keytalk_*.crt" "KeyTalk Demo CCA" ; then
        return 1
    fi
    if ! find_cert_with_cn "/usr/local/share/ca-certificates/keytalk_*.crt" "KeyTalk Demo PCA" ; then
        return 1
    fi


    if [ $# -eq 6 ] ; then
        verify_customization "$4" "$5" "$6"
    fi
}

# Lean & mean provisioning test
# usage: verify_provisioning ${provider1} [${provider2} ...]
function verify_provisioning()
{
    for provider in "$@"
    do
        rm -rf ~/.keytalk/keystore/*.pem
        if ! /usr/local/bin/keytalk/ktclient --provider ${provider} --service CUST_ANO_INTERNAL_TESTUI --user DemoUser ; then
            echo "ERROR: Provisioning test failed. KeyTalk client is not properly configured" >&2
            return 1
        fi

        num=$(ls ~/.keytalk/keystore/*.pem | wc -l)
        if [ "${num}" -ne "1" ] ; then
            echo "ERROR: Provisioning test failed. No or more than one personal certificates received. Actual: ${num}, expected 1" >&2
            return 1
        fi
        for f in ~/.keytalk/keystore/*.pem ; do
            permissions=$(stat --format '%a' "$f")
            if [ "${permissions}" != "400" ]; then
                echo "ERROR: Invalid permission ${permissions} of the personal certificate ${f}. Expected 400" >&2
                return 1
            fi
        done

    done
}

# Problem report generation test
# usage: verify_pr_generation
function verify_pr_generation()
{
    if ! /usr/local/bin/keytalk/ktprgen ; then
        echo "ERROR: PR generation test failed. KeyTalk PR tool finished with error" >&2
        return 1
    fi
    if [ ! -f ~/keytalk.clnt.pr.dat ]; then
        echo "ERROR: PR generation test failed. No PR file generated at ~/keytalk.clnt.pr.dat" >&2
        return 1
    fi

    local pr_files=$(unzip -lqq ~/keytalk.clnt.pr.dat)
    for expected_file in ${PR_FILES_REQUIRED_PATTERN} ; do
        if ! echo ${pr_files} | grep -oq ${expected_file} ; then
            echo "ERROR: PR generation test failed. No ${expected_file} found in ~/keytalk.clnt.pr.dat" >&2
            echo "PR file listing: ${pr_files}"
            return 1
        fi
    done
}


# usage: files=$(_customization_files_required $provider $rccd_type)
function _customization_files_required()
{
    local provider="$1"
    local rccd_type="$2"

    # notice we use '$HOME' because '~' is not expanded in this construct
    local files="$HOME/.keytalk/user.ini"

    if [ x"${rccd_type}" == x"admin" ]; then
        files+=" /etc/keytalk/master.ini"
    fi

    echo ${files}
}

# usage: copy_keytalk_logs_to $dir
function copy_keytalk_logs_to()
{
    local log_dir="$1"

    [ ! -d "${log_dir}" ] && mkdir -p "${log_dir}"
    [ -f ~/.keytalk/ktclient.log ] && cp -f ~/.keytalk/ktclient.log "${log_dir}/"
    if ls ~/tmp/kt*.log > /dev/null 2>&1 ; then
       cp -f ~/tmp/kt*.log "${log_dir}/"
    fi
}

# usage: configure_apache $cas-dir
function configure_apache()
{
    echo "Configuring Apache for test"

    local cas_dir="$1"


    if ! grep -q a.example.com /etc/hosts; then
        echo "127.0.0.1 a.example.com" >> /etc/hosts
    fi
    if ! grep -q b.example.com /etc/hosts; then
        echo "127.0.0.1 b.example.com" >> /etc/hosts
    fi
    if ! grep -q "NameVirtualHost \*:3003" /etc/apache2/apache2.conf; then
        echo "NameVirtualHost *:3003" >> /etc/apache2/apache2.conf
    fi

    cp apache/apache.ini /etc/keytalk/
    cat ${cas_dir}/signingcacert.pem ${cas_dir}/pcacert.pem > apache/localhost-ssl-cert/cas.pem

    # ignore non-existing sites
    a2dissite default 000-default default-ssl || true > /dev/null

    local newline=$'\n'
    local ports_conf="<IfModule ssl_module>"
    for port in 3000 3001 3002 ; do
        ports_conf+="${newline}Listen ${port}"
        local site_config_path=/etc/apache2/sites-available/keytalk-test-${port}-ssl.conf
        sed -E "s/\{\{LISTEN_PORT\}\}/${port}/" apache/ssl.conf.templ > ${site_config_path}
        sed -i -E "s/\{\{SERVER_NAME_DIRECTIVE\}\}//" ${site_config_path}
        sed -i -E "s/\{\{DASH_SERVER_NAME\}\}//" ${site_config_path}
        a2ensite keytalk-test-${port}-ssl.conf > /dev/null
        local certs_file_path=$(grep -E '^\s+SSLCertificateFile\s+' ${site_config_path} | awk '{print $2}')
        local key_file_path=$(grep -E '^\s+SSLCertificateKeyFile\s+' ${site_config_path} | awk '{print $2}')
        cat apache/localhost-ssl-cert/cert.pem apache/localhost-ssl-cert/cas.pem > ${certs_file_path}
        cp -f apache/localhost-ssl-cert/key.pem ${key_file_path}
    done
    ports_conf+="${newline}Listen 3003"
    ports_conf+="${newline}</IfModule>"
    echo "${ports_conf}" > /etc/apache2/ports.conf

    for server_name in "a.example.com" "b.example.com" ; do
        local site_config_path=/etc/apache2/sites-available/keytalk-test-3003-${server_name}-ssl.conf
        sed -E "s/\{\{LISTEN_PORT\}\}/3003/" apache/ssl.conf.templ > ${site_config_path}
        sed -i -E "s/\{\{SERVER_NAME_DIRECTIVE\}\}/ServerName $server_name/" ${site_config_path}
        sed -i -E "s/\{\{DASH_SERVER_NAME\}\}/-$server_name/" ${site_config_path}
        a2ensite keytalk-test-3003-${server_name}-ssl.conf > /dev/null
        local certs_file_path=$(grep -E '^\s+SSLCertificateFile\s+' ${site_config_path} | awk '{print $2}')
        local key_file_path=$(grep -E '^\s+SSLCertificateKeyFile\s+' ${site_config_path} | awk '{print $2}')
        cat apache/localhost-ssl-cert/cert.pem apache/localhost-ssl-cert/cas.pem > ${certs_file_path}
        cp -f apache/localhost-ssl-cert/key.pem ${key_file_path}
    done

    if ! service apache2 restart ; then
        echo "ERROR restarting Apache. Recent apache error log:"
        tail -n 50 /var/log/apache2/error.log
        return 1
    fi
}
