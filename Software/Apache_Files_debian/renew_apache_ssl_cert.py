#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import json
import os
import pwd
import glob
import OpenSSL
import ssl
import socket
import util
import apache_util
import pipes
import platform

# @notice we don't use HOME env. variable ( os.path.expanduser("~") or os.getenv('HOME') ) since when this script gets called with 'sudo'
# it may, depending on the system security policy, give us home directory of the original caller, which is in most cases not what we want.
# For example HOME env. variable (~) is set to target's user in Debian 8
# ('sudo' acts as 'sudo -H') but is preserved in Ubuntu 16 ('sudo' acts as
# 'sudo -E')
HOME_DIR = pwd.getpwuid(os.getuid()).pw_dir
TMP_DIR = os.path.join(HOME_DIR, 'tmp')
KEYSTORE_DIR = os.path.join(HOME_DIR, '.keytalk/keystore')
CONFIG_FILE_PATH = '/etc/keytalk/apache.ini'
KTCLIENT_APP_PATH = '/usr/local/bin/keytalk/ktclient'
LOG_FILE_PATH = os.path.join(TMP_DIR, 'ktapachecertrenewal.log')
CRON_LOG_FILE_PATH = os.path.join(TMP_DIR, 'cron.ktapachecertrenewal.log')
KTCLIENT_LOG_PATH = os.path.join(HOME_DIR, '.keytalk/ktclient.log')
Logger = util.init_logger(
    'keytalk', LOG_FILE_PATH, "KeyTalk Apache certificate renewal", "DEBUG", "INFO")

# Globals
error_messages = []
warning_messages = []
all_error_messages = []
all_warning_messages = []
force_arg = '--force'


def shellquoted_site(site):
    newdict = {}
    for key, val in site.iteritems():
        if isinstance(val, basestring):
            newdict[key] = pipes.quote(val)
        else:
            newdict[key] = val
    return newdict


def is_cert_renewal_needed(site):
    vhost = site['VHost']
    server_name = site.get('ServerName')

    if (not os.path.isfile(apache_util.get_apache_ssl_cert_path(vhost, server_name))) or (
            not os.path.isfile(apache_util.get_apache_ssl_key_path(vhost, server_name))):
        Logger.info(
            "Certificate for {} {} does not exist and needs renewal".format(
                vhost, server_name or ''))
        return True

    host, port = apache_util.parse_connection_address_from_vhost(vhost)
    try:
        pem_cert = ssl.get_server_certificate((host, port))
    except socket.error as e:
        raise Exception(
            'Could not retrieve server certificate from "{}:{}": {}'.format(host, port, e))

    # Check whether the cert is expired
    cert_expired, cert_expiration_utc = util.is_cert_expired(
        pem_cert, vhost, site['KeyTalkProvider'], site['KeyTalkService'], Logger)
    if cert_expired:
        Logger.info("Certificate for {} {} effectively expires at {} UTC and needs renewal".format(
            vhost, server_name or '', cert_expiration_utc))
        return True

    # Check whether the cert is revoked
    if util.is_cert_revoked(pem_cert, Logger):
        Logger.info("Certificate for {} {} has been revoked and needs renewal".format(
            vhost, server_name or ''))
        return True

    # The cert doesn't need renewal
    Logger.info(
        "Certificate for {} {} effectively expires at {} UTC and does not require renewal (run with {} to renew anyway)".format(
            vhost,
            server_name or '',
            cert_expiration_utc,
            force_arg))
    return False


def get_cert(site):
    quoted_site = shellquoted_site(site)
    Logger.info(
        'Retrieving SSL certificate for virtual host at {VHost}. Provider {KeyTalkProvider}, service {KeyTalkService}, user {KeyTalkUser}'.format(
            **site))
    cmd = KTCLIENT_APP_PATH + \
        ' --provider {KeyTalkProvider} --service {KeyTalkService} --user {KeyTalkUser}'.format(
            **quoted_site)
    if site['KeyTalkPassword'] is not None:
        cmd += ' --password {KeyTalkPassword}'.format(**quoted_site)

    try:
        util.run_cmd(cmd, Logger, censored_text_list=[site['KeyTalkPassword']])
    except util.CmdFailedException as ex:
        if ex.retval == util.AUTH_DELAY:
            raise Exception(
                'Authentication to service "{}" of provider "{}" unsuccessful. Invalid credentials, delay before reattempt possible, message: "{}" "{}"'.format(
                    site['KeyTalkService'],
                    site['KeyTalkProvider'],
                    ex.stderr,
                    ex.stdout))
        elif ex.retval == util.AUTH_USER_LOCKED:
            raise Exception(
                'Authentication to service "{}" of provider "{}" unsuccessful. User locked out, message: "{}" "{}"'.format(
                    site['KeyTalkService'],
                    site['KeyTalkProvider'],
                    ex.stderr,
                    ex.stdout))
        elif ex.retval == util.PASSWD_EXPIRED:
            raise Exception(
                'Authentication to service "{}" of provider "{}" unsuccessful. Password expired, message: "{}" "{}"'.format(
                    site['KeyTalkService'],
                    site['KeyTalkProvider'],
                    ex.stderr,
                    ex.stdout))
        else:
            raise
    pem_cert_key_path = max(glob.glob(KEYSTORE_DIR + '/*.pem'), key=os.path.getctime)
    Logger.debug('Retrieved KeyTalk certificate at ' + pem_cert_key_path)
    return pem_cert_key_path


def reload_apache():
    try:
        util.run_cmd('service apache2 status', Logger)
    except util.CmdFailedException as ex:
        if ex.retval == 3:
            return  # Apache inactive, nothing to be done
        else:
            raise
    util.run_cmd('service apache2 reload', Logger)


def install_apache_ssl_cert(pem_cert_key_path, site, restart_apache=False):
    vhost = site['VHost']
    Logger.info('Installing SSL certificate for virtual host at {VHost}'.format(**site))

    server_name = site['ServerName']
    ssl_cert_path = apache_util.get_apache_ssl_cert_path(vhost, server_name)
    ssl_key_path = apache_util.get_apache_ssl_key_path(vhost, server_name)

    certs = util.parse_certs(pem_cert_key_path, Logger)
    if not certs:
        raise Exception(
            "No X.509 certs found in {} received by KeyTalk client".format(pem_cert_key_path))
    keys = util.parse_keys(pem_cert_key_path, Logger)
    if not keys:
        raise Exception(
            "No X.509 keys found in {} received by KeyTalk client".format(pem_cert_key_path))
    cas = util.parse_cas(Logger)

    if util.same_file(ssl_cert_path, ssl_key_path):
        Logger.debug(
            "Saving SSL certificate with key and {} CAs to {}".format(
                len(cas), ssl_cert_path))
        util.save_to_file('\n'.join(certs + keys + cas), ssl_cert_path)
    else:
        Logger.debug(
            "Saving SSL certificates (serial: {}) and {} CAs to {}".format(
                OpenSSL.crypto.load_certificate(
                    OpenSSL.crypto.FILETYPE_PEM,
                    certs[0]).get_serial_number(),
                len(cas),
                ssl_cert_path))
        util.save_to_file('\n'.join(certs + cas), ssl_cert_path)
        Logger.debug("Saving SSL key to " + ssl_key_path)
        util.save_to_file('\n'.join(keys), ssl_key_path)

    # ask Apache to gracefully reload key material
    if restart_apache:
        reload_apache()


def update_apache_config(site):
    vhost = site['VHost']
    server_name = site['ServerName']
    Logger.info('Updating Apache configuration for virtual host at {}{}'.format(
        vhost, ', {}'.format(server_name) if server_name else ''))
    default_cert_path = apache_util.get_apache_ssl_cert_path(vhost, server_name)
    default_key_path = apache_util.get_apache_ssl_key_path(vhost, server_name)
    current_cert_path = apache_util.get_apache_vhost_directive(
        vhost, server_name, 'SSLCertificateFile')
    current_key_path = apache_util.get_apache_vhost_directive(
        vhost, server_name, 'SSLCertificateKeyFile')

    if current_cert_path != default_cert_path:
        apache_util.set_apache_vhost_directive(
            vhost,
            server_name,
            'SSLCertificateFile',
            default_cert_path)

    if current_key_path != default_key_path:
        apache_util.set_apache_vhost_directive(
            vhost,
            server_name,
            'SSLCertificateKeyFile',
            default_key_path)


def log_error(message, summary_message=None):
    error_messages.append(summary_message or message)
    all_error_messages.append(summary_message or message)
    Logger.error(message)


def log_warning(message, summary_message=None):
    warning_messages.append(summary_message or message)
    all_warning_messages.append(summary_message or message)
    Logger.warning(message)


def print_error_summary(errors, warnings):
    if errors:
        Logger.error('Errors summary:')
        for message in errors:
            for line in message.splitlines():
                Logger.error('    %s', line)
        Logger.error(
            'Please check configuration file "%s" and your Apache site configurations.',
            CONFIG_FILE_PATH)

    if warnings:
        Logger.error('Warning summary:')
        for message in warnings:
            for line in message.splitlines():
                Logger.error('    %s', line)
        Logger.error(
            'Please check configuration file "%s" and your Apache configurations.',
            CONFIG_FILE_PATH)


def make_validation_error_message(validation_errors, vhost):
    error_summary = []
    if validation_errors:
        error_summary.append('Errors in VHost "%s" configuration:' % vhost)

        for message in validation_errors:
            error_summary.append('\t' + message)

        error_summary.append(
            'Skipping certificate renewal for VHost "%s" due to configuration errors' %
            vhost)
    return "\n".join(error_summary)


def validate_site_configuration(site, valid_vhosts):
    validation_errors = []
    if site['VHost'] is not None:
        vhost = apache_util.parse_connection_address_from_vhost(site['VHost'])
        if vhost not in valid_vhosts:
            validation_errors.append(
                'Apache VHost "{}:{}" not found. Please check with "apache2ctl -t -D DUMP_VHOSTS".'.format(vhost[0], vhost[1]))

    keytalk_provider = site['KeyTalkProvider']
    keytalk_service = site['KeyTalkService']
    if keytalk_provider and keytalk_provider not in util.get_keytalk_providers():
        validation_errors.append('Unknown KeyTalkProvider "%s".' % (keytalk_provider))
    elif keytalk_service and keytalk_service not in util.get_keytalk_services(keytalk_provider):
        validation_errors.append(
            'Unknown KeyTalkService "%s" for KeyTalkProvider "%s"."' %
            (keytalk_service, keytalk_provider))

    return validation_errors


def email_results(site):
    """
    Email error and warning messages for a specific site.

    :param site: The site configuration with E-mail notification settings (if any)
    """
    if not (site and 'EmailNotifications' in site and site[
            'EmailNotifications'] is True and (error_messages or warning_messages)):
        return

    status = 'SUCCESS'
    if error_messages:
        status = 'ERROR'
    elif warning_messages:
        status = 'WARNINGS'

    message = ''
    log_files = [
        path for path in [
            LOG_FILE_PATH,
            KTCLIENT_LOG_PATH,
            CRON_LOG_FILE_PATH] if os.path.isfile(path)]
    message += 'Server hostname: {}\r\n'.format(platform.node())
    message += 'Apache VHost: {}\r\n'.format(site['VHost'])
    message += 'Apache VHost ServerName: {}\r\n'.format(site['ServerName'] or '<None>')
    message += '\r\n'
    if error_messages:
        message += '== Errors ==\r\n{}'.format('\r\n'.join(error_messages))
    if warning_messages:
        message += '\r\n\r\n'
        message += '== Warnings ==\r\n{}'.format('\r\n'.join(warning_messages))
    message += '\r\n\r\nMore information can be found in the attached log file cutouts.'
    message += '\r\nFor full logs, please see the following files:\r\n\t'
    message += '\r\n\t'.join(log_files)

    subject = '{}: {} (VHost {}{})'.format(
        site['EmailSubject'], status, site['VHost'] or '<unknown>', ', {}'.format(
            site['ServerName']) if site['ServerName'] else '')
    server = site['EmailServer']
    try:
        attachments = []
        for log_file in log_files:
            att_name = os.path.basename(log_file)
            att_content = open(log_file).read()
            # Only send the last 200 lines of the log to save traffic
            att_content = '\n'.join(att_content.splitlines()[-200:])
            attachments.append((att_name, att_content))
        util.send_email(
            server,
            site['EmailFrom'],
            site['EmailTo'].split(','),
            subject,
            message,
            attachments)
    except Exception as e:
        log_error('Could not send e-mail summary for VHost "{}, {}": {} {} {}'.format(site.get('VHost', '<unknown>'),
                                                                                      site.get(
                                                                                          'ServerName', ''),
                                                                                      type(e),
                                                                                      e,
                                                                                      util.format_traceback()),
                  'Could not send e-mail summary for VHost "{}, {}": {} {}'.format(site.get('VHost', '<unknown>'),
                                                                                   site.get(
                                                                                       'ServerName', ''),
                                                                                   type(e),
                                                                                   e))
        return False
    return True


def process_vhost(
        site_configuration,
        current_vhost_string,
        current_vhost_name,
        apache_vhosts,
        force_renew_certs):
    global error_messages
    global warning_messages

    current_site = site_configuration
    try:
        # Validation
        parsed_site, validation_errors = util.parse_settings(
            current_site, util.APACHE_RENEWAL_SETTINGS)
        # parsed_site either contains correctly parsed site settings with populated defaults or None (and a non-empty validation error list).
        # Even if parsing failed, we want to present the user with a complete error list. So for purposes of further validation
        # we fill current_site at least defined values (e.g. None) using
        # populate_defaults such that validate_site_configuration can is able to
        # do its checks.
        current_site = parsed_site or util.populate_defaults(
            current_site, util.APACHE_RENEWAL_SETTINGS)

        validation_errors.extend(validate_site_configuration(current_site, apache_vhosts.keys()))
        if validation_errors:
            message = make_validation_error_message(validation_errors,
                                                    current_vhost_string)
            log_error(message)
            raise Exception(
                'Errors during validation of VHost "{}, {}".'.format(
                    current_vhost_string, current_vhost_name))

        # Processing
        if not apache_util.is_apache_running():
            if (not os.path.isfile(apache_util.get_apache_ssl_cert_path(current_vhost_string, current_vhost_name))) or (
                    not os.path.isfile(apache_util.get_apache_ssl_key_path(current_vhost_string, current_vhost_name))):
                log_warning(
                    'Apache is not running, but certificate/keyfile missing. Attempting to correct. Note: this will not restart apache.')
                pem_cert_key_path = get_cert(current_site)
                install_apache_ssl_cert(pem_cert_key_path, current_site)
                email_results(current_site)
                return True
            else:
                raise Exception(
                    'Apache is not running, skipping certificate update for VHost {} {}.'.format(
                        current_vhost_string, current_vhost_name))

        update_apache_config(current_site)
        if force_renew_certs or is_cert_renewal_needed(current_site):
            pem_cert_key_path = get_cert(current_site)
            install_apache_ssl_cert(pem_cert_key_path, current_site)
    except Exception as e:
        # Log error, but continue processing the next VHost
        log_error(
            'VHost "{}, {}": {} {} {}'.format(
                current_vhost_string,
                current_vhost_name,
                type(e),
                e,
                util.format_traceback()),
            'VHost "{}, {}": {} {}'.format(
                current_vhost_string,
                current_vhost_name,
                type(e),
                e))
        return False
    finally:
        email_results(current_site)
        error_messages = []
        warning_messages = []

    return True


def main():
    try:
        if os.geteuid() != 0:
            Logger.error('{} must be run as root.'.format(sys.argv[0]))
            sys.exit(1)

        current_vhost_string = None
        current_vhost_name = None

        force_renew_certs = len(sys.argv) == 2 and sys.argv[1] == force_arg

        with open(CONFIG_FILE_PATH) as f:
            config = util.strip_json_comments(f.read())
            try:
                sites = json.loads(config)
            except Exception as ex:
                raise Exception(
                    'Could not parse configuration file "{}": {}'.format(
                        CONFIG_FILE_PATH, ex))

        apache_vhosts = apache_util.get_apache_vhosts()
        if len(apache_vhosts) == 0:
            Logger.error(
                'Could not find any Apache VHosts on this system, please configure your Apache VHosts before using this script.')
            sys.exit(2)

        for site_index, site in enumerate(sites):
            current_vhost_string = site.get(
                'VHost', 'VHost configuration number {}'.format(
                    site_index + 1))
            current_vhost_name = site.get('ServerName', '')
            process_vhost(
                site,
                current_vhost_string,
                current_vhost_name,
                apache_vhosts,
                force_renew_certs)

        if apache_util.is_apache_running():
            reload_apache()

    except Exception as e:
        log_error(
            'VHost "{}, {}": {} {} {}'.format(
                current_vhost_string,
                current_vhost_name,
                type(e),
                e,
                util.format_traceback()),
            'VHost "{}, {}": {} {}'.format(
                current_vhost_string,
                current_vhost_name,
                type(e),
                e))
        sys.exit(1)
    finally:
        print_error_summary(all_error_messages, all_warning_messages)
        util.close_logger(Logger)
        if all_error_messages:
            sys.exit(1)


#
# Entry point
#
if __name__ == "__main__":
    main()
