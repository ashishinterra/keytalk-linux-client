#!/usr/bin/env python
# -*- coding: utf-8 -*-

import re
import glob
from lxml import etree
import util


def parse_apache_configs(configuration_files):
    """
    Return a combined (XML) tree with the content of the specified apache configuration files.

    For example:
        root
            ConfigFile (apache.a.conf)
                IfModule (mod_ssl.c)
                    VirtualHost (localhost:3000)
                        ServerName (a.example.com)
                        SSLCertificateFile (my.file.pem)
                        ...
                    VirtualHost (localhost:3001)
                        ServerName (b.example.com)
                        ...
            ConfigFile (apache.b.conf)
                IfModule (mod_ssl.c)
                    VirtualHost (localhost:3002)
                        ...
                    VirtualHost (localhost:3003)
                        ...
    :param configuration_files: A list of apache configuration files
    :returns: an lxml etree with the contents of all specified configuration files
    """
    configs = etree.Element('root')
    for file_path in configuration_files:
        child_node = etree.SubElement(configs, 'ConfigFile')
        etree.SubElement(child_node, 'Path').text = file_path
        child_node.append(parse_apache_config(open(file_path).read(), file_path))
    return configs


# Simple parser of Apache config files
# Based on http://www.poldylicious.de/system/files/apacheconfig.py.txt

def parse_apache_config(config_string, file_path_hint):
    """
    Return a list of (XML) tree nodes for the specified virtual host.

    See the example the IfModule node in parse_apache_configs for the format).
    """
    re_comment = re.compile(r"""^#.*$""")
    re_section_start = re.compile(r"""^<(?P<name>[^/\s>]+)\s*(?P<value>[^>]+)?>$""")
    re_section_end = re.compile(r"""^</(?P<name>[^\s>]+)\s*>$""")

    root = etree.Element('root')
    element = root
    element_stack = []

    for line_number, line in enumerate(config_string.splitlines(), start=1):
        line = line.strip()
        if (len(line) == 0) or re_comment.match(line):
            continue

        match = re_section_start.match(line)
        if match:
            element = etree.SubElement(element, match.group("name"))
            values = match.group("value").split()
            etree.SubElement(element, 'SectionValue').text = ' '.join(values)
            etree.SubElement(element, 'StartLine').text = str(line_number)
            element_stack.append(match.group("name"))
            continue
        match = re_section_end.match(line)
        if match:
            if element_stack and element_stack[-1] != match.group("name"):
                raise Exception('{}:{}: Section mismatch: "{}" should be "{}"'.format(
                    file_path_hint or '<unknown file>', line_number, match.group("name"), element_stack[-1]))
            etree.SubElement(element, 'EndLine').text = str(line_number)
            element = element.getparent()
            element_stack.pop()
            continue
        values = [(item[1:-1] if item[0] == '"' and item[-1] == '"' else item)
                  for item in line.split()]
        directive = etree.SubElement(element, values[0])
        directive.text = ' '.join(values[1:])
        etree.SubElement(directive, 'StartLine').text = str(line_number)
        etree.SubElement(directive, 'EndLine').text = str(line_number)
    return root


def get_available_apache_config_files():
    if util.has_executable('apache2'):
        return sorted([file_path for file_path in glob.glob(
            '/etc/apache2/sites-available/*') if not util.is_backup_file_path(file_path)])
    elif util.has_executable('httpd'):
        return sorted(glob.glob('/etc/httpd/conf.d/*'))
    else:
        raise Exception('Unable to find available apache config files')


def is_backup_file_path(path):
    re.match(r'.*\.orig\.[0-9]+[-][0-9]+', path)


def get_enabled_apache_config_files():
    if util.has_executable('apache2'):
        return sorted(glob.glob('/etc/apache2/sites-enabled/*'))
    elif util.has_executable('httpd'):
        return sorted(glob.glob('/etc/httpd/conf.d/*.conf'))
    else:
        raise Exception('Unable to find enabled apache configurations')


def get_apache_vhosts():
    """
    Return connection addresses of apache virtual hosts and their server names.

    For example:
      {
        ('localhost', 3000) : []                                 # IP-based virtual host
        ('localhost', 3001) : ['a.example.com', 'b.example.com'] # Two name-based virtual hosts
      }
    """
    configs = parse_apache_configs(get_enabled_apache_config_files())
    vhosts = {}
    for vhost in configs.xpath('//VirtualHost'):
        connection_address = parse_connection_address_from_vhost(vhost.find('SectionValue').text)
        if connection_address not in vhosts:
            vhosts[connection_address] = []
        server_name_node = vhost.find('ServerName')
        if server_name_node is not None:
            vhosts[connection_address].append(server_name_node.text)
    return vhosts


def get_apache_vhost_directive(
        vhost,
        server_name,
        directive,
        config_files=get_available_apache_config_files()):
    """
    Update the directive in a vhost config element or add it if it does not exist.

    :param vhost: string representing a vhost
    :param server_name: The name of the virtual host (in case of an IP-base VHost this should be "None")
    :param directive: the name of the directive to be retrieved
    """
    vh = get_vhost_config(config_files, vhost, server_name)
    directive_elements = vh.xpath(directive)
    if len(directive_elements) is not 1:
        config_files = set([d.xpath('ancestor::ConfigFile/Path')
                            [0].text for d in directive_elements])
        raise Exception(
            'Expected exactly 1 occurrence of directive "{}" in VHost "{}", found {} occurrences in configuration file(s) {}.'.format(
                directive,
                vhost,
                len(directive_elements),
                ', '.join(config_files)))
    directive = directive_elements[0]
    if directive is not None:
        return directive.text
    raise Exception('No directive "{}" found for vhost "{}"'.format(directive, vhost))


def set_apache_vhost_directive(
        vhost,
        server_name,
        directive,
        new_value,
        config_files=get_available_apache_config_files()):
    """
    Update/add the directive in vhost config

    :param vhost: string representing a vhost
    :param server_name: The name of the virtual host (in case of an IP-base VHost this should be "None")
    :param directive: the name of the directive to be written or added
    :param new_value: the value of the directive to be written or added
    """
    vhost_config = get_vhost_config(config_files, vhost, server_name)
    if vhost_config is None:
        raise Exception('No VHost "{}" found'.format(vhost))

    new_line_content = '\t\t{} {}\n'.format(directive, new_value)
    directive_elements = vhost_config.xpath(directive)
    if len(directive_elements) > 1:
        config_files = set([d.xpath('ancestor::ConfigFile/Path')
                            [0].text for d in directive_elements])
        raise Exception(
            'Expected at most 1 occurrence of directive "{}" in VHost "{}", found {} occurrences in configuration file(s) {}.'.format(
                directive,
                vhost,
                len(directive_elements),
                ','.join(config_files)))

    config_path = vhost_config.xpath('ancestor::ConfigFile/Path')[0].text
    config_content = open(config_path, 'r').readlines()
    if len(directive_elements) == 0:
        config_content.insert(int(vhost_config.find('EndLine').text) - 1, new_line_content)
    else:
        directive_element = directive_elements[0]
        config_content[int(directive_element.find('StartLine').text) - 1] = new_line_content
    util.save_to_file("".join(config_content), config_path, backup=True)


def get_vhost_config(config_files, vhost, server_name=None):
    """
    Return a list of (XML) tree nodes for the specified virtual host.

    See the example VirtualHost node in parse_apache_configs for the format.
    :param config_files: A list of apache config files
    :param vhost: A virtual host string (e.g. "localhost:443")
    :server_name: The apache ServerName (domain name) in case of named virtual hosts
    """
    configs = parse_apache_configs(config_files)
    vhosts = configs.xpath('//VirtualHost')
    searched_vhost_address = parse_connection_address_from_vhost(vhost)
    found_vhost_sections = []
    for vh in vhosts:
        vhost_address = parse_connection_address_from_vhost(vh.find('SectionValue').text)
        if vhost_address == searched_vhost_address:
            found_vhost_sections.append(vh)

    results = []
    if not server_name:
        results = found_vhost_sections
    else:
        for vhost_section in found_vhost_sections:
            found_server_name = vhost_section.find('ServerName')
            if found_server_name is not None and found_server_name.text == server_name:
                results.append(vhost_section)

    if not results:
        raise Exception(
            'Vhost "{}", with name "{}" not found in configuration files {}'.format(
                vhost, server_name, ', '.join(config_files)))
    elif len(results) != 1:
        config_files_with_matches = set(
            [v.xpath('ancestor::ConfigFile/Path')[0].text for v in results])
        raise Exception(
            'Expected exactly 1 occurrence of VHost "{}, {}". Found {} occurrences in configuration file(s) {}. If these are named VHosts, please specify the ServerName.'.format(
                vhost,
                server_name,
                len(results),
                ', '.join(config_files_with_matches)))
    else:
        return results[0]


def get_apache_ssl_cert_path(vhost, server_name):
    if server_name:
        return '/etc/ssl/certs/keytalk-apache-{}-{}-ssl.pem'.format(
            parse_connection_address_from_vhost(vhost)[1], server_name)
    else:
        return '/etc/ssl/certs/keytalk-apache-{}-ssl.pem'.format(
            parse_connection_address_from_vhost(vhost)[1])


def is_apache_running():
    try:
        util.run_cmd("pgrep -x apache2")
    except util.CmdFailedException:
        return False
    return True


def get_apache_ssl_key_path(vhost, server_name):
    if server_name:
        return '/etc/ssl/private/keytalk-apache-{}-{}-ssl.key'.format(
            parse_connection_address_from_vhost(vhost)[1], server_name)
    else:
        return '/etc/ssl/private/keytalk-apache-{}-ssl.key'.format(
            parse_connection_address_from_vhost(vhost)[1])


def is_apache_port(port_string):
    return port_string.isdigit() or port_string.strip() == '*'


def parse_connection_address_from_vhost(vhost_string):
    """
    Return the connection address of the specified VHost string.

    Examples:
        "localhost:3000" -> (localhost, 3000)
        "localhost" -> (localhost, 443)
        "localhost:something" -> (localhost, 443)
    :parm vhost_string: A string representing the connection address of a virtual host (e.g. localhost:3000)
    """
    groups = vhost_string.split(':')
    if len(groups) == 1 or not is_apache_port(groups[-1]):
        host, port = ':'.join(groups), 443
    else:
        host, port = ':'.join(groups[:-1]), groups[-1]

    host = "localhost" if host in ("*", "_default_") else host
    try:
        port = int(port)
    except Exception:        # best-effort
        port = 443
    return (host, port)
