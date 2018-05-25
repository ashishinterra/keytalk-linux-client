#!/usr/bin/env python
# -*- coding: utf-8 -*-​

import json
import os
from pipes import quote
import sys
import zipfile
from tempfile import NamedTemporaryFile
import copy
import subprocess
import tempfile
import glob
import imp
import shutil
import re


INSTALLER_DIR_PREFIX = 'keytalkclient-'


def run_cmd(cmd, logger=None):
    """
    Invoke a command and return the standard output on completion.

    In case of errors, exceptions are thrown.
    :param cmd: the command to be executed
    """
    if logger:
        logger.debug("Executing command: " + str(cmd))

    try:
        p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except Exception as e:
        raise Exception("Failed to execute {}. {}".format(cmd, e))

    try:
        retval = p.wait()
    except Exception as e:
        raise Exception("Failed to wait for {} finishes. {}".format(cmd, e))

    stdout = p.stdout.read().decode('utf-8').strip()
    stderr = p.stderr.read().decode('utf-8').strip()
    if retval == 0:
        return stdout

    raise CmdFailedException(cmd, retval, stdout, stderr)


class CmdFailedException(Exception):

    def __init__(self, cmd, retval, stdout, stderr):
        super(
            CmdFailedException,
            self).__init__(
            u"{} finished with code {}. Stdout: {}. Stderr: {}".format(
                cmd,
                retval,
                stdout,
                stderr))
        self.cmd = cmd
        self.retval = retval
        self.stdout = stdout
        self.stderr = stderr

    def format_indented_message(self, message):
        lines = []
        for line in message.splitlines():
            lines += [line]
        lines += ['    Stderr:']
        for line in self.stderr.splitlines():
            lines += ['        {}'.format(line)]
        lines += ['    Stdout:']
        for line in self.stdout.splitlines():
            lines += ['        {}'.format(line)]
        return '\n'.join(lines)


def run_remote_cmd(host, command, connect_timeout=5, only_stdout=False):
    return run_cmd('ssh -o ConnectTimeout={} {} {}{}'.format(int(connect_timeout),
                                                             quote(host), quote(command), ' 2>&1' if only_stdout else ''))


def print_usage():
    print('Usage:')
    print(
        '    {} install <vhosts_config.ini> <keytalk_installer.tgz> <ktclient.rccd>'.format(
            sys.argv[0]))
    print('    {} remove <root@remotehost> [<root@remotehost2>, ...]'.format(sys.argv[0]))
    print('    {} remove <vhosts_config.ini>'.format(sys.argv[0]))


def parse_install_args():
    if len(sys.argv) != 5:
        print_usage()
        sys.exit(1)

    config_path = sys.argv[2]
    installer_path = sys.argv[3]
    rccd_path = sys.argv[4]
    if not os.path.exists(config_path):
        raise Exception('Specified config path "{}" does not exist.'.format(config_path))

    if not os.path.exists(installer_path) or INSTALLER_DIR_PREFIX not in run_cmd(
            'tar tfv {}'.format(quote(installer_path))):
        raise Exception('Specified installer "{}" does not exist.'.format(installer_path))

    if not os.path.exists(rccd_path):
        raise Exception('Specified RCCD path "{}" does not exist.'.format(rccd_path))

    try:
        with zipfile.ZipFile(rccd_path, 'r') as z:
            if 'content/user.ini' not in z.namelist():
                raise Exception('Specified file is not a valid RCCD file')
    except Exception:
        raise Exception('Specified file is not a valid RCCD file')

    return {'command': 'install',
            'installer_path': installer_path,
            'config_path': config_path,
            'rccd_path': rccd_path}


def parse_remove_args():
    if len(sys.argv) < 3:
        print_usage()
        sys.exit(1)

    if len(sys.argv) == 3 and os.path.isfile(sys.argv[2]):
        return {'command': 'remove',
                'config_path': sys.argv[2]}
    else:
        return {'command': 'remove',
                'ssh_hosts': sys.argv[2:]}


def parse_args():
    """:returns: A dict containing the parsed options (keys) and their values."""
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)

    command = sys.argv[1]
    if command not in ['install', 'remove']:
        print_usage()
        sys.exit(1)

    if command == 'install':
        return parse_install_args()
    elif command == 'remove':
        return parse_remove_args()
    else:
        raise Exception('Unknown command: "{}".'.format(command))


def validate_sites(sites, util):
    """:returns: A list of error messages found during validation."""
    known_settings = copy.deepcopy(util.APACHE_RENEWAL_SETTINGS)
    known_settings['RemoteHost'] = {'required': True,
                                    'dependencies': []}
    error_messages = []
    for site_index, site in enumerate(sites):
        _, errors = util.parse_settings(site, known_settings)
        vhost = site.get('VHost', 'VHost number {}'.format(site_index + 1))
        server_name = site.get('ServerName', '')
        if errors:
            error_messages.append('Errors in VHost {} {}:'.format(vhost, server_name))
            for error in errors:
                error_messages.append('    ' + error)
    return error_messages


def deploy_site_config(ssh_host, config_path, installer_path, rccd_path):
    """:returns: An error message upon failure or None on success."""
    try:
        remote_temp_dir = run_remote_cmd(ssh_host, 'mktemp -d')
        run_cmd(
            'scp {installer} {rccd} {config} {ssh_host}:{temp_dir}'.format(
                installer=quote(installer_path),
                rccd=quote(rccd_path),
                config=quote(config_path),
                ssh_host=quote(ssh_host),
                temp_dir=quote(remote_temp_dir)))

        run_remote_cmd(
            ssh_host, """set -e;
                         set -x;
                         echo "Checking if apache2 is installed"
                         which apache2
                         (
                             cd {temp_dir} &&
                             tar xfz {installer_filename} &&
                             (
                               cd keytalkclient-* &&
                                ./install.sh
                             ) &&
                             /usr/local/bin/keytalk/ktconfig --rccd-path {rccd_filename} &&
                             cp {config_filename} /etc/keytalk/apache.ini &&
                             /usr/local/bin/keytalk/renew_apache_ssl_cert --force &&
                             echo "*  *  *  *  *   root    /usr/local/bin/keytalk/renew_apache_ssl_cert > $HOME/tmp/cron.ktapachecertrenewal.log 2>&1" > /etc/cron.d/keytalk
                         ) &&
                         rm -rf {temp_dir}""".format(
                temp_dir=quote(remote_temp_dir), installer_filename=quote(
                    os.path.basename(installer_path)), rccd_filename=quote(
                    os.path.basename(rccd_path)), config_filename=quote(
                        os.path.basename(config_path))), only_stdout=True)
    except CmdFailedException as ex:
        return ex.format_indented_message('Could not deploy to {}:'.format(ssh_host))
    return None


def remote_uninstall(ssh_host):
    """:returns: An error message upon failure or None on success."""
    print('Uninstalling client on {}'.format(ssh_host))
    sys.stdout.flush()
    try:
        run_remote_cmd(ssh_host, '/usr/local/bin/keytalk/uninstall_keytalk', only_stdout=True)
    except CmdFailedException as ex:
        print('ERROR')
        return ex.format_indented_message('Could not uninstall on {}:'.format(ssh_host))
    print('OK')
    return None


def create_temp_config_file(sites):
    """:returns: Path to a created (temporary) vhost configuration file."""
    my_sites = copy.deepcopy(sites)

    for my_site in my_sites:
        del my_site['RemoteHost']

    file_name = None
    with NamedTemporaryFile(delete=False) as f:
        file_content = json.dumps(my_sites, sort_keys=True, indent=4)
        f.write(file_content)
        file_name = f.name
    return file_name


def print_errors(errors):
    if errors:
        print('The configuration file contains the following errors:')
        for message in errors:
            print('    {}'.format(message))


def strip_json_comments(s):
    """Remove one-line comments starting with # or // from JSON-like content and return valid JSON."""
    # we intentionally substitute comment with empty line i.o. removing them
    # in order to preserve line numbers when reporting errors by JSON parser further on
    return re.sub(r"(?m)^\s*(#|//).*$", "", s)


def parse_sites_per_remote_host(config_path, util):
    """:returns: Dict containing remote hosts and a list of VHosts to be deployed to this remote host."""
    # Parse and validate sites
    with open(config_path) as f:
        config = strip_json_comments(f.read())
        try:
            sites = json.loads(config)
        except Exception as ex:
            raise Exception(
                'Could not parse configuration template "{}": {}'.format(
                    config_path, ex))

    errors = validate_sites(sites, util)
    print_errors(errors)
    if errors:
        sys.exit(1)

    # Collect sites per remote host
    remote_host_sites = {}
    for site in sites:
        remote_host = site['RemoteHost']
        if remote_host not in remote_host_sites:
            remote_host_sites[remote_host] = []
        remote_host_sites[remote_host].append(site)

    return remote_host_sites


def remote_deploy(config_path, installer_path, rccd_path):
    # Load utils module from installation package
    errors = []
    temp_dir = tempfile.mkdtemp()
    try:
        run_cmd('tar xfv {} -C {}'.format(quote(installer_path), quote(temp_dir)))
        installer_dir = glob.glob('{}/{}*'.format(temp_dir, INSTALLER_DIR_PREFIX))[0]
        util = imp.load_source('ktinstaller_util', '{}/util.py'.format(installer_dir))
    finally:
        shutil.rmtree(temp_dir)

    # Validate sites using imported util module
    remote_host_sites = parse_sites_per_remote_host(config_path, util)

    # deploy sites per remote host
    for remote_host, host_sites in remote_host_sites.iteritems():
        print('Deploying sites for {}'.format(remote_host))
        sys.stdout.flush()
        # Generate based on the "raw" (but validated) site instead of a parsed/populated one
        # Reason: prevent introduction of null values in the JSON file
        site_temp_config = create_temp_config_file(host_sites)
        error_message = deploy_site_config(
            remote_host, site_temp_config, installer_path, rccd_path)
        if error_message:
            print('ERROR')
            errors.append(error_message)
            uninstall_error = remote_uninstall(remote_host)
            if uninstall_error:
                errors.append(uninstall_error)
        else:
            print('OK')
        os.remove(site_temp_config)

    if errors:
        print('Errors during remote deployment:')
        indented_messages = []
        for message in errors:
            indented_message = '    ' + '\n    '.join(message.splitlines())
            indented_messages.append(indented_message)

        print('\n\n\n\n'.join(indented_messages))
        sys.exit(1)


def remote_uninstall_hosts(ssh_hosts):
    errors = []
    for host in ssh_hosts:
        error = remote_uninstall(host)
        if error:
            errors.append(error)

    if errors:
        print('Errors during remote uninstall:')
        indented_messages = []
        for message in errors:
            indented_message = '    ' + '\n    '.join(message.splitlines())
            indented_messages.append(indented_message)

        print('\n\n\n\n'.join(indented_messages))
        sys.exit(1)


def remote_uninstall_from_config(config_path):
    with open(config_path) as f:
        config = strip_json_comments(f.read())
        try:
            sites = json.loads(config)
        except Exception as ex:
            raise Exception(
                'Could not parse vhosts configuration file "{}": {}'.format(
                    config_path, ex))

    remote_hosts = []
    for site in sites:
        if 'RemoteHost' in site:
            remote_hosts.append(site['RemoteHost'])

    remote_uninstall_hosts(remote_hosts)


def main():
    args = parse_args()

    if args['command'] == 'install':
        remote_deploy(args['config_path'], args['installer_path'], args['rccd_path'])
    if args['command'] == 'remove' and 'ssh_hosts' in args:
        remote_uninstall_hosts(args['ssh_hosts'])
    if args['command'] == 'remove' and 'config_path' in args:
        remote_uninstall_from_config(args['config_path'])


#
# Entry point
#
if __name__ == "__main__":
    main()
