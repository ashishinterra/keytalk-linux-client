#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import unittest
import os
import time
import datetime
import uuid

sys.path.insert(0, '/usr/local/bin/keytalk')
import util


class TestUtil(unittest.TestCase):

    def test_run_cmd_with_success(self):
        self.assertEquals(util.run_cmd('pwd'), os.getcwd())

    def test_run_cmd_with_error(self):
        self.assertRaises(Exception, util.run_cmd, 'nonexistent_command')

    def test_strip_json_comments(self):
        json_with_comments = '''
        [
        # number one
        'first',
        // number two
        2,
        "three"
        ]
        '''
        expected_json = '''
        [

        'first',

        2,
        "three"
        ]
        '''
        self.assertEquals(util.strip_json_comments(json_with_comments), expected_json)

    def test_parse_certs_with_success(self):
        self.assertEquals(len(util.parse_certs('3certs.pem')), 3)
        self.assertEquals(len(util.parse_certs('2certs1key.pem')), 2)

    def test_parse_certs_with_error(self):
        self.assertEquals(util.parse_certs('/non/existing/file/name'), [])
        self.assertEquals(util.parse_certs('3privkeys.pem'), [])

    def test_parse_keys_with_success(self):
        self.assertEquals(len(util.parse_keys('3privkeys.pem')), 3)
        self.assertEquals(len(util.parse_keys('2certs1key.pem')), 1)

    def test_parse_keys_with_error(self):
        self.assertEquals(util.parse_keys('/non/existing/file/name'), [])
        self.assertEquals(util.parse_keys('3certs.pem'), [])

    def test_same_file_with_success(self):
        # same file
        self.assertTrue(util.same_file("/etc/keytalk/apache.ini", "/etc/keytalk/apache.ini"))
        # whitespace
        self.assertTrue(
            util.same_file("/etc/keytalk/apache.ini    ", " /etc/keytalk/apache.ini "))
        # symlink
        os.system("ln -sf /etc/keytalk/apache.ini /tmp/apache-slink.ini")
        self.assertTrue(util.same_file("/etc/keytalk/apache.ini", "/tmp/apache-slink.ini"))
        # hardlink
        os.system("ln -f /etc/keytalk/apache.ini /tmp/apache-hlink.ini")
        self.assertTrue(util.same_file("/etc/keytalk/apache.ini", "/tmp/apache-hlink.ini"))

    def test_same_file_with_error(self):
        # different files
        self.assertFalse(util.same_file("/etc/keytalk/apache.ini", "/etc/keytalk/resept.ini"))
        # different files (though same contents)
        os.system("cp -f /etc/keytalk/apache.ini /tmp/apache.ini")
        self.assertFalse(util.same_file("/etc/keytalk/apache.ini", "/tmp/apache.ini"))
        # directory
        self.assertFalse(util.same_file("/etc/keytalk/", "/etc/keytalk/"))
        # not a file
        self.assertFalse(util.same_file("", ""))

    def test_get_cert_validity_percentage_with_success(self):
        self.assertEqual(util.get_cert_validity_percentage(
                         "DemoProvider", "CUST_PASSWD_INTERNAL"), 10)

    def test_get_cert_validity_percentage_with_error(self):
        self.assertRaises(
            Exception,
            util.get_cert_validity_percentage,
            "DemoProvider",
            "invalid-service")
        self.assertRaises(
            Exception,
            util.get_cert_validity_percentage,
            "invalid-provider",
            "CUST_PASSWD_INTERNAL")

    def test_get_keytalk_providers(self):
        self.assertEqual(util.get_keytalk_providers(), ['DemoProvider'])

    def test_get_keytalk_services(self):
        demo_provider_services = ["CUST_PASSWD_INTERNAL",
                                  "CUST_CR_MYSQL",
                                  "CUST_ANO_INTERNAL_TESTUI",
                                  ]
        self.assertEqual(
            sorted(
                util.get_keytalk_services('DemoProvider')),
            sorted(demo_provider_services))

    def test_censor_string(self):
        self.assertEqual(
            util.censor_string(
                'Some secret text with lots of secrets',
                ['secret']),
            'Some <erased> text with lots of <erased>s')
        self.assertEqual(util.censor_string('Some secret text with lots of hidden secrets', [
                         'secret', 'hidden']), 'Some <erased> text with lots of <erased> <erased>s')
        self.assertEqual(
            util.censor_string(
                'Some !@#$%^&*()\\/ text with lots of !@#$%^&*()\\/s',
                ['!@#$%^&*()\\/']),
            'Some <erased> text with lots of <erased>s')

    def test_populate_defaults(self):
        # given
        known_settings = {'VHost': {'required': True,
                                    'dependencies': []},

                          'ServerName': {'required': False,
                                         'dependencies': []},

                          'EmailNotifications': {'required': False,
                                                 'dependencies': [],
                                                 'default_value': False},

                          'EmailSubject': {'required': False,
                                           'dependencies': ['EmailNotifications'],
                                           'default_value': 'Apache certificate renewal'},

                          'EmailSubjectPostfix': {'required': False,
                                                  'dependencies': ['EmailSubject'],
                                                  'default_value': 'some_postfix'}}

        # whens/thens
        settings_dict = util.populate_defaults({}, known_settings)
        self.assertEquals(settings_dict, {'VHost': None,
                                          'ServerName': None,
                                          'EmailNotifications': False,
                                          'EmailSubject': None,
                                          'EmailSubjectPostfix': None})

        settings_dict = util.populate_defaults({'EmailNotifications': True}, known_settings)
        self.assertEquals(settings_dict, {'VHost': None,
                                          'ServerName': None,
                                          'EmailNotifications': True,
                                          'EmailSubject': 'Apache certificate renewal',
                                          'EmailSubjectPostfix': None})

        settings_dict = util.populate_defaults(
            {'EmailNotifications': True, 'EmailSubject': 'Some subject'}, known_settings)
        self.assertEquals(settings_dict, {'VHost': None,
                                          'ServerName': None,
                                          'EmailNotifications': True,
                                          'EmailSubject': 'Some subject',
                                          'EmailSubjectPostfix': 'some_postfix'})

    def test_validate_setting_dependencies(self):
        # given
        known_settings = {'Required': {'required': True,
                                       'dependencies': []},

                          'Optional': {'required': False,
                                       'dependencies': []},

                          'RequiredWithDependency': {'required': True,
                                                     'dependencies': ['Optional']},

                          'OptionalWithDependency': {'required': False,
                                                     'dependencies': ['Optional']}}
        # whens/thens
        errors = util.validate_setting_dependencies(
            {
                'Required': 'value',
                'Optional': 'value2',
                'RequiredWithDependency': 'value3',
                'OptionalWithDependency': 'value4'},
            known_settings)
        self.assertEqual(errors, [])

        errors = util.validate_setting_dependencies({'Required': 'value'}, known_settings)
        self.assertEqual(errors, [])

        errors = util.validate_setting_dependencies(
            {'Required': 'value', 'Optional': 'value2', 'RequiredWithDependency': 'value3'}, known_settings)
        self.assertEqual(errors, [])

        errors = util.validate_setting_dependencies({}, known_settings)
        self.assertEqual(errors, ['Required setting "Required" not found.'])

        errors = util.validate_setting_dependencies(
            {'Required': 'value', 'Optional': 'value2'}, known_settings)
        self.assertEqual(
            errors,
            ['The current configuration requires setting "RequiredWithDependency".'])

    def test_parse_settings(self):
        known_settings = {
            'Required': {
                'required': True,
                'dependencies': [],
                'default_value': 'not_used_since_required'},
            'Optional': {
                'required': False,
                'dependencies': [],
                'default_value': 'optional_default'},
            'RequiredWithDependency': {
                'required': True,
                'dependencies': ['Optional'],
                'default_value': 'not_used_since_required'},
            'OptionalWithDependency': {
                'required': False,
                'dependencies': ['Optional']},
            'OptionalWithDependency2': {
                'required': False,
                'dependencies': ['OptionalWithDependency'],
                'default_value': 'optional_with_dependency_2_default'}}

        # whens/thens
        settings, errors = util.parse_settings({'Required': 'value', 'Optional': 'value2', 'RequiredWithDependency': 'value3',
                                                'OptionalWithDependency': 'value4', 'OptionalWithDependency2': 'value5'}, known_settings)
        self.assertEqual(errors, [])
        self.assertEquals(settings, {'Required': 'value',
                                     'Optional': 'value2',
                                     'RequiredWithDependency': 'value3',
                                     'OptionalWithDependency': 'value4',
                                     'OptionalWithDependency2': 'value5'})

        settings, errors = util.parse_settings({'Required': 'value'}, known_settings)
        self.assertEqual(errors, [])
        self.assertEquals(settings, {'Required': 'value',
                                     'Optional': 'optional_default',
                                     'RequiredWithDependency': None,
                                     'OptionalWithDependency': None,
                                     'OptionalWithDependency2': None})

        settings, errors = util.parse_settings(
            {'Required': 'value', 'Optional': 'value2', 'RequiredWithDependency': 'value3'}, known_settings)
        self.assertEqual(errors, [])
        self.assertEquals(settings, {'Required': 'value',
                                     'Optional': 'value2',
                                     'RequiredWithDependency': 'value3',
                                     'OptionalWithDependency': None,
                                     'OptionalWithDependency2': None})

        settings, errors = util.parse_settings(
            {'Required': 'value', 'Optional': 'value2', 'RequiredWithDependency': 'value3', 'OptionalWithDependency': 'value4'}, known_settings)
        self.assertEqual(errors, [])
        self.assertEquals(settings,
                          {'Required': 'value',
                           'Optional': 'value2',
                           'RequiredWithDependency': 'value3',
                           'OptionalWithDependency': 'value4',
                           'OptionalWithDependency2': 'optional_with_dependency_2_default'})

        settings, errors = util.parse_settings({}, known_settings)
        self.assertEqual(errors, ['Required setting "Required" not found.'])
        self.assertEquals(settings, None)

        settings, errors = util.parse_settings(
            {'Required': 'value', 'Optional': 'value2'}, known_settings)
        self.assertEqual(
            errors,
            ['The current configuration requires setting "RequiredWithDependency".'])
        self.assertEquals(settings, None)

        settings, errors = util.parse_settings(
            {'Required': 'value', 'Optional': 'value2', 'RequiredWithDependency': 'value3',
             'OptionalWithDependency': 'value4', 'OptionalWithDependency2': 'value5', 'UnknownSetting': 'value6'}, known_settings)
        self.assertEqual(len(errors), 1)
        self.assertRegexpMatches(errors[0], 'Unknown setting "UnknownSetting" encountered')
        self.assertEquals(settings, None)

    def test_email(self):
        # given
        mailbox_dir = '/var/mail'
        random_string = str(uuid.uuid4())
        subject = 'Test Mail ' + random_string
        msg_body = 'Message body: ' + random_string
        attachments = [('attachment1-name-' + random_string, 'attachment1-body'),
                       ('attachment2-name-' + random_string, 'attachment2-body')]
        # when
        util.send_email(
            smtp_server_addr='localhost',
            sender='root@localhost',
            recipients=['root@localhost'],
            subject=subject,
            message=msg_body,
            attachments=attachments)

        # then
        time.sleep(2)
        self.assertTrue(os.listdir(mailbox_dir))
        mailbox_file_name = util.run_cmd('ls -Art /var/mail| tail -n 1')
        mail_file = mailbox_dir + '/' + mailbox_file_name
        mails = open(mail_file).read()

        self.assertTrue('From: root@localhost' in mails)
        self.assertTrue('To: root@localhost' in mails)
        self.assertTrue('Subject: ' + subject in mails)
        self.assertTrue(msg_body in mails)
        self.assertTrue('filename=\\"{}\\"";'.format(attachments[0][0]) in mails)
        self.assertTrue('filename=\\"{}\\"";'.format(attachments[1][0]) in mails)

    def test_is_backup_file_path(self):
        self.assertFalse(util.is_backup_file_path('/home/me/some-file_path'))
        self.assertFalse(util.is_backup_file_path('some-file_path'))
        self.assertFalse(util.is_backup_file_path('some-file_path.orig'))
        self.assertFalse(util.is_backup_file_path('some-file_path.orig.'))
        self.assertFalse(util.is_backup_file_path('some-file_path.orig.something-something'))
        self.assertTrue(util.is_backup_file_path('some-file_path.orig.1-2'))
        self.assertTrue(util.is_backup_file_path('some-file_path.orig.0000-0000'))
        self.assertTrue(util.is_backup_file_path('some-file_path.orig.20150928-140919'))


if __name__ == '__main__':
    unittest.main()
