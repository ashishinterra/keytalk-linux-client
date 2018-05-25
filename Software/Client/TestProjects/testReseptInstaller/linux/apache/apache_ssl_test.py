#!/usr/bin/env python
# -*- coding: utf-8 -*-


import urllib2
import unittest
import time
import ssl
import OpenSSL
import os
import re
from subprocess import Popen, PIPE

"""
To run specific test case:
python -m unittest test_module_name.TestClass.test_method
"""


class TestApacheSsl(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        if os.system('service apache2 status > /dev/null 2>&1 || service apache2 start') != 0:
            raise Exception("Failed to start Apache")

    @classmethod
    def tearDownClass(cls):
        pass

    def test_ssl_cert_is_renewal_for_multiple_vhosts(self):
        # given
        renew_ssl_cert_cmd = '/usr/local/bin/keytalk/renew_apache_ssl_cert'
        force_renew_ssl_cert_cmd = renew_ssl_cert_cmd + " --force"
        ca_file = 'localhost-ssl-cert/cas.pem'
        vhost_configurations = [
            {'host': 'localhost', 'port': 3000, 'renew_enabled': True},
            {'host': 'localhost', 'port': 3001, 'renew_enabled': False},
            {'host': 'localhost', 'port': 3002, 'renew_enabled': True},
            {'host': 'a.example.com', 'port': 3003, 'renew_enabled': True},
            {'host': 'b.example.com', 'port': 3003, 'renew_enabled': True},
        ]
        # when
        print("Forcibly renewing Apache SSL certificates")
        p = Popen(force_renew_ssl_cert_cmd, stdout=PIPE, stderr=PIPE, shell=True)
        # then
        out, err = p.communicate()
        self.assertEquals(p.returncode, 0, "Stdout: {}, Stderr: {}".format(out, err))
        for vhost in vhost_configurations:
            print("Checking {host}:{port}, renew enabled: {renew_enabled}".format(**vhost))
            conn = urllib2.urlopen('https://{host}:{port}'.format(**vhost), cafile=ca_file)
            html_contents = conn.read()
            self.assertIsNotNone(
                re.search(
                    "<h1>(It works!|Index of /)</h1>",
                    html_contents),
                html_contents)
            cert = ssl.get_server_certificate((vhost['host'], vhost['port']))
            # save site state for further testing
            vhost['html_contents'] = html_contents
            vhost['x509'] = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM, cert)

        # when
        print("Renewing valid Apache SSL certificates")
        # sleep for at least a second to notice certificate time difference
        time.sleep(1)
        p = Popen(renew_ssl_cert_cmd, stdout=PIPE, stderr=PIPE, shell=True)
        # then we expect no certificates get renewed because they are still valid
        out, err = p.communicate()
        self.assertEquals(p.returncode, 0, "Stdout: {}, Stderr: {}".format(out, err))
        for vhost in vhost_configurations:
            print("Checking {host}:{port}, renew enabled: {renew_enabled}".format(**vhost))
            conn = urllib2.urlopen('https://{host}:{port}'.format(**vhost), cafile=ca_file)
            html_contents = conn.read()
            cert = ssl.get_server_certificate((vhost['host'], vhost['port']))
            x509 = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM, cert)
            self.assertEquals(html_contents, vhost['html_contents'])
            self.assertEquals(x509.get_subject(), vhost['x509'].get_subject())
            self.assertEquals(x509.get_issuer(), vhost['x509'].get_issuer())
            self.assertEquals(x509.get_serial_number(), vhost['x509'].get_serial_number())
            self.assertEquals(x509.digest(b"sha1"), vhost['x509'].digest(b"sha1"))
            self.assertEquals(x509.get_notBefore(), vhost['x509'].get_notBefore())
            self.assertEquals(x509.get_notAfter(), vhost['x509'].get_notAfter())

        # when
        print("Forcibly renewing Apache SSL certificates")
        # sleep for at least a second to notice certificate time difference
        time.sleep(1)
        p = Popen(force_renew_ssl_cert_cmd, stdout=PIPE, stderr=PIPE, shell=True)
        # then we expect the certificates will be renewed because we enforce that
        out, err = p.communicate()
        self.assertEquals(p.returncode, 0, "Stdout: {}, Stderr: {}".format(out, err))
        for vhost in vhost_configurations:
            print("Checking {host}:{port}, renew enabled: {renew_enabled}".format(**vhost))
            conn = urllib2.urlopen('https://{host}:{port}'.format(**vhost), cafile=ca_file)
            html_contents = conn.read()
            cert = ssl.get_server_certificate((vhost['host'], vhost['port']))
            x509 = OpenSSL.crypto.load_certificate(OpenSSL.crypto.FILETYPE_PEM, cert)
            self.assertEquals(html_contents, vhost['html_contents'])
            self.assertEquals(x509.get_subject(), vhost['x509'].get_subject())
            self.assertEquals(x509.get_issuer(), vhost['x509'].get_issuer())
            if vhost['renew_enabled']:
                # certificate serial, digest and validity should change
                self.assertNotEquals(x509.get_serial_number(), vhost['x509'].get_serial_number())
                self.assertNotEquals(x509.digest(b"sha1"), vhost['x509'].digest(b"sha1"))
                self.assertGreater(time.strptime(x509.get_notBefore(), "%Y%m%d%H%M%SZ"),
                                   time.strptime(vhost['x509'].get_notBefore(), "%Y%m%d%H%M%SZ"))
                self.assertGreater(time.strptime(x509.get_notAfter(), "%Y%m%d%H%M%SZ"),
                                   time.strptime(vhost['x509'].get_notAfter(), "%Y%m%d%H%M%SZ"))
            else:
                # no cert change expected
                self.assertEquals(x509.get_serial_number(), vhost['x509'].get_serial_number())
                self.assertEquals(x509.digest(b"sha1"), vhost['x509'].digest(b"sha1"))
                self.assertEquals(x509.get_notBefore(), vhost['x509'].get_notBefore())
                self.assertEquals(x509.get_notAfter(), vhost['x509'].get_notAfter())


if __name__ == '__main__':
    unittest.main()
