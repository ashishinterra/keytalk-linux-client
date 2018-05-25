#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import unittest
import glob
import tempfile
import shutil

sys.path.insert(0, '/usr/local/bin/keytalk')
import apache_util


class TestApacheUtil(unittest.TestCase):

    def test_parse_connection_address_from_vhost(self):
        self.assertEqual(apache_util.parse_connection_address_from_vhost(
            "192.168.1.1:3001"), ("192.168.1.1", 3001))
        self.assertEqual(apache_util.parse_connection_address_from_vhost(
            "*:3001"), ("localhost", 3001))
        self.assertEqual(apache_util.parse_connection_address_from_vhost(
            "_default_:8080"), ("localhost", 8080))
        self.assertEqual(apache_util.parse_connection_address_from_vhost(
            "_default_:*"), ("localhost", 443))
        self.assertEqual(apache_util.parse_connection_address_from_vhost(
            "192.168.1.1"), ("192.168.1.1", 443))
        self.assertEqual(apache_util.parse_connection_address_from_vhost(
            "[2001:4860:4860::8888]"), ("[2001:4860:4860::8888]", 443))
        self.assertEqual(apache_util.parse_connection_address_from_vhost(
            "[2001:4860:4860::8888]:8443"), ("[2001:4860:4860::8888]", 8443))

    def test_get_apache_vhosts(self):
        self.assertEquals(apache_util.get_apache_vhosts(), {('localhost', 3003): ['a.example.com', 'b.example.com'],
                                                            ('localhost', 3001): [], ('localhost', 3002): [],
                                                            ('localhost', 3000): []})

    def test_apache_config_parsing(self):
        apache_config = "\n".join(
            [
                "<IfModule mod_ssl.c>",
                "	<VirtualHost _default_:3003>",
                "      ServerName a.example.com",
                "		DocumentRoot /var/www/html",
                "		ErrorLog ${APACHE_LOG_DIR}/error.log",
                "		CustomLog ${APACHE_LOG_DIR}/access.log combined",
                "		SSLEngine on",
                "		SSLCertificateFile	/etc/ssl/certs/keytalk-test-3000-ssl.pem",
                "		SSLCertificateKeyFile /etc/ssl/private/keytalk-test-3000-ssl.key",
                "        # note: SSLCertificateChainFile became obsolete with Apache-2.4.8 in favor of SSLCertificateFile",
                "	</VirtualHost>",
                "</IfModule>"])

        conf = apache_util.parse_apache_config(apache_config, '<test content>')
        self.assertEqual([s.text for s in conf.xpath('IfModule/SectionValue')], ['mod_ssl.c'])
        self.assertEqual([s.text for s in conf.xpath(
            'IfModule/VirtualHost/SectionValue')], ['_default_:3003'])
        self.assertEqual([s.text for s in conf.xpath(
            'IfModule/VirtualHost/ServerName')], ['a.example.com'])
        self.assertEqual([s.text for s in conf.xpath('IfModule/VirtualHost/CustomLog')],
                         ['${APACHE_LOG_DIR}/access.log combined'])
        self.assertEqual([s.text for s in conf.xpath(
            'IfModule/VirtualHost/SSLCertificateFile')], ['/etc/ssl/certs/keytalk-test-3000-ssl.pem'])

        self.assertEqual([int(s.text) for s in conf.xpath('IfModule/StartLine')], [1])
        self.assertEqual([int(s.text) for s in conf.xpath('IfModule/EndLine')], [12])
        self.assertEqual([int(s.text) for s in conf.xpath('IfModule/VirtualHost/StartLine')], [2])
        self.assertEqual([int(s.text) for s in conf.xpath('IfModule/VirtualHost/EndLine')], [11])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost/ServerName/StartLine')], [3])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost/ServerName/EndLine')], [3])

    def test_apache_config_parsing_with_multiple_vhosts(self):
        apache_config = "\n".join(
            [
                "<IfModule mod_ssl.c>",
                "	<VirtualHost _default_:3003>",
                "      ServerName a.example.com",
                "		DocumentRoot /var/www/html",
                "		ErrorLog ${APACHE_LOG_DIR}/error.log",
                "		CustomLog ${APACHE_LOG_DIR}/access.log combined",
                "		SSLEngine on",
                "		SSLCertificateFile	/etc/ssl/certs/keytalk-test-3003-ssl-a.example.com.pem",
                "		SSLCertificateKeyFile /etc/ssl/private/keytalk-test-3003-ssl-a.example.com.key",
                "        # note: SSLCertificateChainFile became obsolete with Apache-2.4.8 in favor of SSLCertificateFile",
                "	</VirtualHost>",
                "	<VirtualHost _default_:3004>",
                "        ServerName b.example.com",
                "		DocumentRoot /var/www/html",
                "		ErrorLog ${APACHE_LOG_DIR}/error.log",
                "		CustomLog ${APACHE_LOG_DIR}/access.log combined",
                "		SSLEngine on",
                "		SSLCertificateFile	/etc/ssl/certs/keytalk-test-3004-ssl-b.example.com.pem",
                "		SSLCertificateKeyFile /etc/ssl/private/keytalk-test-3004-ssl-b.example.com.key",
                "        # note: SSLCertificateChainFile became obsolete with Apache-2.4.8 in favor of SSLCertificateFile",
                "	</VirtualHost>",
                "</IfModule>"])

        conf = apache_util.parse_apache_config(apache_config, '<test content>')
        self.assertEqual([s.text for s in conf.xpath('IfModule/SectionValue')], ['mod_ssl.c'])
        self.assertEqual([s.text for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="a.example.com"]/SectionValue')], ['_default_:3003'])
        self.assertEqual([s.text for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="b.example.com"]/SectionValue')], ['_default_:3004'])
        self.assertEqual([s.text for s in conf.xpath(
            'IfModule/VirtualHost/ServerName')], ['a.example.com', 'b.example.com'])
        self.assertEqual([s.text for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="a.example.com"]/CustomLog')], ['${APACHE_LOG_DIR}/access.log combined'])
        self.assertEqual(
            [
                s.text for s in conf.xpath('IfModule/VirtualHost[ServerName/text()="a.example.com"]/SSLCertificateFile')],
            ['/etc/ssl/certs/keytalk-test-3003-ssl-a.example.com.pem'])
        self.assertEqual(
            [
                s.text for s in conf.xpath('IfModule/VirtualHost[ServerName/text()="b.example.com"]/SSLCertificateFile')],
            ['/etc/ssl/certs/keytalk-test-3004-ssl-b.example.com.pem'])

        self.assertEqual([int(s.text) for s in conf.xpath('IfModule/StartLine')], [1])
        self.assertEqual([int(s.text) for s in conf.xpath('IfModule/EndLine')], [22])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="a.example.com"]/StartLine')], [2])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="a.example.com"]/EndLine')], [11])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="a.example.com"]/ServerName/StartLine')], [3])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="a.example.com"]/ServerName/EndLine')], [3])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="b.example.com"]/StartLine')], [12])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="b.example.com"]/EndLine')], [21])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="b.example.com"]/ServerName/StartLine')], [13])
        self.assertEqual([int(s.text) for s in conf.xpath(
            'IfModule/VirtualHost[ServerName/text()="b.example.com"]/ServerName/EndLine')], [13])

    def test_get_enabled_apache_config_files(self):
        expected_enabled_apache_config_files = [
            '/etc/apache2/sites-enabled/keytalk-test-3000-ssl.conf',
            '/etc/apache2/sites-enabled/keytalk-test-3001-ssl.conf',
            '/etc/apache2/sites-enabled/keytalk-test-3002-ssl.conf',
            '/etc/apache2/sites-enabled/keytalk-test-3003-a.example.com-ssl.conf',
            '/etc/apache2/sites-enabled/keytalk-test-3003-b.example.com-ssl.conf']
        self.assertEqual(
            apache_util.get_enabled_apache_config_files(),
            expected_enabled_apache_config_files)

    def test_get_available_apache_config_files(self):
        import os
        if os.system('apache2 -v | grep -q "Server version: Apache/2.2"') == 0:
            # lagacy Apache v2.2
            expected_available_apache_config_files = [
                '/etc/apache2/sites-available/default',
                '/etc/apache2/sites-available/default-ssl']
        else:
            # (presumably) modern Apache 2.4+
            expected_available_apache_config_files = [
                '/etc/apache2/sites-available/000-default.conf',
                '/etc/apache2/sites-available/default-ssl.conf']

        expected_available_apache_config_files += [
            '/etc/apache2/sites-available/keytalk-test-3000-ssl.conf',
            '/etc/apache2/sites-available/keytalk-test-3001-ssl.conf',
            '/etc/apache2/sites-available/keytalk-test-3002-ssl.conf',
            '/etc/apache2/sites-available/keytalk-test-3003-a.example.com-ssl.conf',
            '/etc/apache2/sites-available/keytalk-test-3003-b.example.com-ssl.conf']
        self.assertEqual(
            apache_util.get_available_apache_config_files(),
            expected_available_apache_config_files)

    def test_get_apache_vhost_directive(self):
        conf_files = glob.glob('./keytalk-test-*.conf')
        self.assertEquals(
            apache_util.get_apache_vhost_directive(
                '*:3000',
                None,
                'SSLCertificateFile',
                config_files=conf_files),
            '/etc/ssl/certs/keytalk-test-3000-ssl.pem')
        self.assertEquals(
            apache_util.get_apache_vhost_directive(
                '*:3001',
                None,
                'SSLCertificateFile',
                config_files=conf_files),
            '/etc/ssl/certs/keytalk-test-3001-ssl.pem')
        self.assertEquals(
            apache_util.get_apache_vhost_directive(
                '*:3002',
                None,
                'SSLCertificateFile',
                config_files=conf_files),
            '/etc/ssl/certs/keytalk-test-3002-ssl.pem')

        # No occurrence, single occurence, multiple occurrence
        self.assertEquals(
            apache_util.get_apache_vhost_directive(
                '*:3000',
                None,
                'TestSingleDirective',
                config_files=conf_files),
            'a')
        self.assertRaises(
            Exception,
            apache_util.get_apache_vhost_directive,
            '*:3003',
            None,
            'TestDoubleDirective',
            config_files=conf_files)
        self.assertRaises(
            Exception,
            apache_util.get_apache_vhost_directive,
            '*:3003',
            None,
            'NonexistingDirective',
            config_files=conf_files)
        self.assertRaises(
            Exception,
            apache_util.get_apache_vhost_directive,
            '*:9999',
            None,
            'TestSingleDirective',
            config_files=conf_files)

        # Name-based vhosts
        self.assertRaises(
            Exception,
            apache_util.get_apache_vhost_directive,
            '*:3003',
            None,
            'SSLCertificateFile',
            config_files=conf_files)
        self.assertRaises(
            Exception,
            apache_util.get_apache_vhost_directive,
            '*:3000',
            'a.example.com',  # Not name-based
            'SSLCertificateFile',
            config_files=conf_files)
        self.assertEquals(
            apache_util.get_apache_vhost_directive(
                '*:3003',
                'a.example.com',
                'SSLCertificateFile',
                config_files=conf_files),
            '/etc/ssl/certs/keytalk-test-3003-a.example.com-ssl.pem')
        self.assertEquals(
            apache_util.get_apache_vhost_directive(
                '*:3003',
                'b.example.com',
                'SSLCertificateFile',
                config_files=conf_files),
            '/etc/ssl/certs/keytalk-test-3003-b.example.com-ssl.pem')
        self.assertRaises(
            Exception,
            apache_util.get_apache_vhost_directive,
            '*:3003',
            'doesnotexist.example.com',
            'SSLCertificateFile',
            config_files=conf_files)

    def test_set_apache_vhost_directive(self):
        temp_dir = None
        try:
            temp_dir = tempfile.mkdtemp()
            for file_path in glob.glob('./keytalk-test-*.conf'):
                shutil.copy(file_path, temp_dir)
            conf_files = glob.glob(temp_dir + '/keytalk-test-*.conf')
            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3000',
                    None,
                    'SSLCertificateFile',
                    config_files=conf_files),
                '/etc/ssl/certs/keytalk-test-3000-ssl.pem')

            apache_util.set_apache_vhost_directive(
                '*:3000',
                None,
                'SSLCertificateFile',
                '/some/path',
                config_files=conf_files)

            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3000',
                    None,
                    'SSLCertificateFile',
                    config_files=conf_files),
                '/some/path')

            # No occurrence, single occurence, multiple occurrence
            self.assertRaises(
                Exception,
                apache_util.get_apache_vhost_directive,
                '*:3003',
                None,
                'ToBeCreatedDirective',
                config_files=conf_files)
            apache_util.set_apache_vhost_directive(
                '*:3000',
                None,
                'ToBeCreatedDirective',
                'some_value',
                config_files=conf_files)
            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3000',
                    None,
                    'ToBeCreatedDirective',
                    config_files=conf_files),
                'some_value')

            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3000',
                    None,
                    'TestSingleDirective',
                    config_files=conf_files),
                'a')
            apache_util.set_apache_vhost_directive(
                '*:3000', None, 'TestSingleDirective', 'aaa', config_files=conf_files)
            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3000',
                    None,
                    'TestSingleDirective',
                    config_files=conf_files),
                'aaa')

            self.assertRaises(
                Exception,
                apache_util.set_apache_vhost_directive,
                '*:3003',
                None,
                'TestDoubleDirective',
                'some_value',
                config_files=conf_files)

            # Name-based vhosts
            self.assertRaises(
                Exception,
                apache_util.set_apache_vhost_directive,
                '*:3003',
                None,
                'SSLCertificateFile',
                '/some/path',
                config_files=glob.glob('./keytalk-test-*.conf'))

            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3003',
                    'a.example.com',
                    'SSLCertificateFile',
                    config_files=conf_files),
                '/etc/ssl/certs/keytalk-test-3003-a.example.com-ssl.pem')
            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3003',
                    'b.example.com',
                    'SSLCertificateFile',
                    config_files=conf_files),
                '/etc/ssl/certs/keytalk-test-3003-b.example.com-ssl.pem')

            apache_util.set_apache_vhost_directive(
                '*:3003',
                'a.example.com',
                'SSLCertificateFile',
                '/some/path/a',
                config_files=conf_files)
            apache_util.set_apache_vhost_directive(
                '*:3003',
                'b.example.com',
                'SSLCertificateFile',
                '/some/path/b',
                config_files=conf_files)

            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3003',
                    'a.example.com',
                    'SSLCertificateFile',
                    config_files=conf_files),
                '/some/path/a')
            self.assertEquals(
                apache_util.get_apache_vhost_directive(
                    '*:3003',
                    'b.example.com',
                    'SSLCertificateFile',
                    config_files=conf_files),
                '/some/path/b')
        finally:
            if temp_dir is not None:
                shutil.rmtree(temp_dir)


if __name__ == '__main__':
    unittest.main()
