#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# Sample Proof-of-concept KeyTalk client capable of retrieving a certificate from KeyTalk 5.x server using different authentication methods
#

# Requires Python 3.4 and higher

######################################################################################
# RCDPv2 flow.
# Goes over HTTPS.
# HTTP GET requests.
# HTTP JSON responses.
#
# client ---------> hello -----------------> server
# client <--------- hello <----------------- server
# client ---------> handshake -------------> server
# client <--------- handshake <------------- server
# client ---------> auth-requirements -----> server
# client <--------- auth-requirements <----- server
# client ---------> authentication --------> server
# client <--------- auth-result <----------- server
# client ---------> last-messages ---------> server
# client <--------- last-messages <--------- server
# client ---------> cert ------------------> server
# client <--------- cert <------------------ server
# client ---------> eoc -------------------> server
# client <--------- eoc <------------------- server

# eoc (end-of-communication) and error can be sent by any party at any time
##########################################################################################

import http.client
import ssl
import urllib.request
import urllib.parse
import urllib.error
import json
import datetime
import base64
import pprint
import hashlib
import socket

import common_config as conf


###################################################################################

#
# Global settings
#
VERBOSE = False

# HACK to bypass checking KeyTalk server hostname during SSL handshake;
# useful for quick testing e.g. when KeyTalk server is IP address iso FQDN
BYPASS_HTTPS_VALIDATION = True

#
# These settings should come from RCCD
#
if BYPASS_HTTPS_VALIDATION:
    KEYTALK_SERVER = '192.168.33.111'
else:
    KEYTALK_SERVER = 'demo.keytalkdemo.com'

SERVER_VERIFICATION_CA_CHAIN = [
    'commcacert.pem',
    'pcacert.pem'
]
LAST_MESSAGES_FROM_UTC = None

UMTS_USERNAME = 'UMTS_2_354162120787078'
GSM_USERNAME = 'GSM_2_354162120787078'


##############################################################################

def debug(msg):
    if VERBOSE:
        print(msg)


def log(msg):
    print(msg)


class KeyTalkProtocol(object):

    def __init__(self):
        self.version = conf.RCDP_VERSION_2_1
        self.conn = None
        self.cookie = None

    #
    # Private API
    #

    @staticmethod
    def _parse_rcdp_response(conn, request_name, expected_status):
        response = conn.getresponse()
        response_payload = response.read().decode()
        if response.status != 200:
            raise Exception(
                'Unexpected response HTTP status {} received on {} request.'.format(
                    response.status, request_name))

        payload = json.loads(response_payload)
        debug("{} -> {} {}.\n{}".format(request_name, response.status,
                                        response.reason, pprint.pformat(payload)))

        status = payload[conf.RCDPV2_RESPONSE_PARAM_NAME_STATUS]
        if status != expected_status:
            if status == conf.RCDPV2_RESPONSE_ERROR:
                code = int(payload[conf.RCDPV2_RESPONSE_PARAM_NAME_ERROR_CODE])
                info = payload[conf.RCDPV2_RESPONSE_PARAM_NAME_ERROR_DESCRIPTION]
                if code == int(conf.RCDP_ERR_CODE["ErrTimeOutOfSync"]):
                    delta_seconds = int(info)
                    if delta_seconds > 0:
                        raise Exception(
                            'Client time is {} seconds ahead the server time'.format(delta_seconds))
                    else:
                        raise Exception(
                            'Client time is {} seconds behind the server time'.format(-delta_seconds))
                else:
                    raise Exception(
                        'Received error {} for response on {}. Extra info: {}'.format(
                            code, request_name, info))
            else:
                raise Exception(
                    'Expected {} response on {} request but received {} instead'.format(
                        expected_status, request_name, status))

        cookie = response.getheader('set-cookie', None)
        return (payload, cookie)

    @staticmethod
    def _request_simple_url(url):
        debug("Opening URL " + url)
        try:
            with urllib.request.urlopen(url) as response:
                payload = response.read()
                return payload
        except Exception as e:
            log("Error opening URL {}. {}".format(url, e))
            return None

    @staticmethod
    def _is_true(dict, key):
        return (key in dict) and (dict[key].lower() == 'true')

    @staticmethod
    def _calc_hwsig(formula):
        #@todo implement for real for the given formula
        return "HWSIG-123456"

    @staticmethod
    def _get_system_hwdescription():
        #@todo implement for real
        return "Windows 7, BIOS s/n 1234567890"

    @staticmethod
    def _resolve_host(uri):
        hostname = urllib.parse.urlparse(uri).hostname
        log("Resolving " + hostname)
        ips = []
        try:
            for addr in socket.getaddrinfo(
                    hostname,
                    port=None,
                    family=socket.AF_INET,
                    type=socket.SOCK_STREAM):
                ips.append(addr[4][0])
        except Exception:
            pass
        try:
            for addr in socket.getaddrinfo(
                    hostname,
                    port=None,
                    family=socket.AF_INET6,
                    type=socket.SOCK_STREAM):
                ips.append(addr[4][0])
        except Exception:
            pass
        if not ips:
            raise Exception("Failed to resolve " + hostname)
        return ips

    @staticmethod
    def _calc_digest(path):
        with open(path, 'rb') as f:
            return hashlib.sha256(f.read()).hexdigest()

    @staticmethod
    def is_cr_authentication(auth_requirements):
        return conf.CRED_RESPONSE in auth_requirements[conf.RCDPV2_RESPONSE_PARAM_NAME_CRED_TYPES]

    @staticmethod
    def is_password_expiring(password_validity_sec):
        """quick&dirty but good enough for tests; see ta::SyfInfo::isUserPasswordExpiring() for proper implementation"""
        seconds_in_day = 24 * 60 * 60
        max_validity_days = 7
        return password_validity_sec >= 0 and\
            password_validity_sec < max_validity_days * seconds_in_day

    @staticmethod
    def calc_responses(username, challenges, response_names):
        if username == UMTS_USERNAME:
            # expect only 2 challenges and 3 responses
            responses = [
                {
                    conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[0],
                    conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "02020202020202020202020202020202"
                },
                {
                    conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[1],
                    conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "03030303030303030303030303030303"
                },
                {
                    conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[2],
                    conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "04040404040404040404040404040404"
                }
            ]
        elif username == GSM_USERNAME:
            # 3 rounds of request-response
            if challenges['GSM RANDOM'] == '101112131415161718191a1b1c1d1e1f':
                responses = [
                    {
                        conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[0],
                        conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "d1d2d3d4"
                    },
                    {
                        conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[1],
                        conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "a0a1a2a3a4a5a6a7"
                    }
                ]
            elif challenges['GSM RANDOM'] == '202122232425262728292a2b2c2d2e2f':
                responses = [
                    {
                        conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[0],
                        conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "e1e2e3e4"
                    },
                    {
                        conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[1],
                        conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "b0b1b2b3b4b5b6b7"
                    }
                ]
            elif challenges['GSM RANDOM'] == '303132333435363738393a3b3c3d3e3f':
                responses = [
                    {
                        conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[0],
                        conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "f1f2f3f4"
                    },
                    {
                        conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_names[1],
                        conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: "c0c1c2c3c4c5c6c7"
                    }
                ]
        else:
            # expect only challenge and one response;
            challenge_value = next(iter(challenges.values()))
            response_name = response_names[0]
            response_value = hashlib.sha1(
                (username + challenge_value).encode()).hexdigest()[:8].upper()
            responses = [
                {
                    conf.RCDPV2_REQUEST_PARAM_NAME_NAME: response_name,
                    conf.RCDPV2_REQUEST_PARAM_NAME_VALUE: response_value
                }
            ]

        return {conf.RCDPV2_REQUEST_PARAM_NAME_RESPONSES: json.dumps(responses)}

    @staticmethod
    def request_auth_credentials(auth_requirements, username, password=None, pincode=None):
        required_cred_types = auth_requirements[conf.RCDPV2_RESPONSE_PARAM_NAME_CRED_TYPES]
        creds = {}
        if conf.CRED_USERID in required_cred_types:
            creds[conf.CRED_USERID] = username
        if conf.CRED_PASSWD in required_cred_types:
            creds[conf.CRED_PASSWD] = password
        if conf.CRED_PIN in required_cred_types:
            creds[conf.CRED_PIN] = pincode
        if conf.CRED_HWSIG in required_cred_types:
            creds[conf.CRED_HWSIG] = KeyTalkProtocol._calc_hwsig(
                auth_requirements[conf.RCDPV2_RESPONSE_PARAM_NAME_HWSIG_FORMULA])

        service_uris = auth_requirements.get(conf.RCDPV2_RESPONSE_PARAM_NAME_SERVICE_URIS, None)

        if KeyTalkProtocol._is_true(auth_requirements,
                                    conf.RCDPV2_RESPONSE_PARAM_NAME_RESOLVE_SERVICE_URIS):
            ips = []
            for service_uri in service_uris:
                ips.append({conf.RCDPV2_REQUEST_PARAM_NAME_URI: service_uri,
                            conf.RCDPV2_REQUEST_PARAM_NAME_IPS: KeyTalkProtocol._resolve_host(service_uri)})
            creds[conf.RCDPV2_REQUEST_PARAM_NAME_RESOLVED] = json.dumps(ips)

        if KeyTalkProtocol._is_true(auth_requirements,
                                    conf.RCDPV2_RESPONSE_PARAM_NAME_CALC_SERVICE_URIS_DIGEST):
            digests = []
            for service_uri in service_uris:
                digests.append({conf.RCDPV2_REQUEST_PARAM_NAME_URI: service_uri,
                                conf.RCDPV2_REQUEST_PARAM_NAME_DIGEST: KeyTalkProtocol._calc_digest(service_uri)})
            creds[conf.RCDPV2_REQUEST_PARAM_NAME_DIGESTS] = json.dumps(digests)

        return creds

    @staticmethod
    def _save_cert(cert, passphrase, format):
        pass_path = 'certkey-password.txt'
        if format == conf.CERT_FORMAT_PEM:
            cert_path = 'cert.pem'
        elif format == conf.CERT_FORMAT_P12:
            cert_path = 'cert.pfx'
        else:
            raise Exception('Unexpected certificate format ' + format)

        with open(cert_path, 'wb') as cert_file:
            cert_file.write(cert)
        with open(pass_path, 'w') as pass_file:
            pass_file.write(passphrase)

        log("The certificate has been saved to {}, passphrase has been saved to {}".format(
            cert_path, pass_path))
        if format == conf.CERT_FORMAT_PEM:
            log("Use the following command if you with to decrypt the key:\nopenssl rsa -in {} -out key.pem -passin pass:{}".format(
                cert_path, passphrase))
        elif format == conf.CERT_FORMAT_P12:
            log("Use the following command if you wish to decrypt the PKCS#12 package:\nopenssl pkcs12 -nodes -in {} -out certkey.pem -passin pass:{}".format(
                cert_path, passphrase))

        return cert_path, pass_path

    def _request(self, action, params={}, send_cookie=True):
        url = "/{}/{}/{}?{}".format(conf.RCDPV2_HTTP_REQUEST_URI_PREFIX,
                                    self.version,
                                    action,
                                    urllib.parse.urlencode(params))
        if VERBOSE:
            self.conn.set_debuglevel(1)
        if send_cookie:
            self.conn.request("GET", url, headers={"Cookie": self.cookie})
        else:
            self.conn.request("GET", url)

    def _get_cert_passphrase(self):
        parsed_cookie = self.cookie.split('=')
        if len(parsed_cookie) != 2 or parsed_cookie[0] != conf.RCDPV2_HTTP_SID_COOKIE_NAME:
            raise Exception('Cannot parse RCDP session cookie from ' + self.cookie)
        sid = parsed_cookie[1]
        passphrase = sid[:conf.RCDP_PACKAGED_CERT_EXPORT_PASSWDSIZE]
        return passphrase

    @staticmethod
    def _load_verification_ca_chain():
        cadata = ''
        for file in SERVER_VERIFICATION_CA_CHAIN:
            cadata += open(file).read()
        return cadata

    #
    # Public API
    #

    def eoc(self):
        self._request(conf.RCDPV2_REQUEST_EOC)

    def hello(self):
        log("Connecting to KeyTalk server at " + KEYTALK_SERVER + "...")

        ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLSv1_2)
        # without explicitly supplying EC curve SSL connection will fail on python-3.5.3
        # with SSL Handshake error
        ssl_ctx.set_ecdh_curve('secp384r1')

        if BYPASS_HTTPS_VALIDATION:
            ssl_ctx.verify_mode = ssl.CERT_NONE
        else:
            ssl_ctx.verify_mode = ssl.CERT_REQUIRED
            ssl_ctx.load_verify_locations(cadata=KeyTalkProtocol._load_verification_ca_chain())

        conn = http.client.HTTPSConnection(KEYTALK_SERVER, context=ssl_ctx)

        self.conn = conn
        request_params = {
            conf.RCDPV2_REQUEST_PARAM_NAME_CALLER_APP_DESCRIPTION: 'Test KeyTalk Python client'}
        self._request(conf.RCDPV2_REQUEST_HELLO, request_params, send_cookie=False)
        response_payload, cookie = KeyTalkProtocol._parse_rcdp_response(
            conn, conf.RCDPV2_REQUEST_HELLO, conf.RCDPV2_RESPONSE_HELLO)
        if response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_VERSION] != self.version:
            raise Exception(
                'Unexpected response received on hello request: {}'.format(response_payload))
        self.cookie = cookie

    def handshake(self):
        request_params = {conf.RCDPV2_REQUEST_PARAM_NAME_CALLER_UTC:
                          datetime.datetime.utcnow().isoformat() + 'Z'}
        self._request(conf.RCDPV2_REQUEST_HANDSHAKE, request_params)
        response_payload, _ = KeyTalkProtocol._parse_rcdp_response(
            self.conn, conf.RCDPV2_REQUEST_HANDSHAKE, conf.RCDPV2_RESPONSE_HANDSHAKE)
        return response_payload

    def get_auth_requirements(self, service):
        request_params = {conf.RCDPV2_REQUEST_PARAM_NAME_SERVICE: service}
        self._request(conf.RCDPV2_REQUEST_AUTH_REQUIREMENTS, request_params)
        response_payload, _ = KeyTalkProtocol._parse_rcdp_response(
            self.conn, conf.RCDPV2_REQUEST_AUTH_REQUIREMENTS, conf.RCDPV2_RESPONSE_AUTH_REQUIREMENTS)
        return response_payload

    def authenticate(self, creds, service=None):
        """authenticate with the given set of credentials
        return password-validity-in-seconds when ok is received or -1 if the password is expired
        return (challenges, response-names) when challenge is received
        raise exception otherwise"""
        if service is not None:
            request_params = {
                conf.RCDPV2_REQUEST_PARAM_NAME_SERVICE: service,
                conf.RCDPV2_REQUEST_PARAM_NAME_CALLER_HW_DESCRIPTION: KeyTalkProtocol._get_system_hwdescription(),
            }
            request_params.update(creds)
        else:
            # normally this means we already sent service on the first authentication
            # request and now on challenge phase
            request_params = creds

        debug("Sending authentication request: " + pprint.pformat(request_params))

        self._request(conf.RCDPV2_REQUEST_AUTHENTICATION, request_params)
        response_payload, _ = KeyTalkProtocol._parse_rcdp_response(
            self.conn, conf.RCDPV2_REQUEST_AUTHENTICATION, conf.RCDPV2_RESPONSE_AUTH_RESULT)
        auth_status = response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_AUTH_STATUS]
        if auth_status == conf.AUTH_OK:
            if conf.RCDPV2_RESPONSE_PARAM_NAME_PASSWORD_VALIDITY in response_payload:
                password_validity = int(
                    response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_PASSWORD_VALIDITY])
            else:
                password_validity = None
            log("Authenticated successfully")
            return password_validity
        elif auth_status == conf.AUTH_CHALLENGE:
            log("Challenge received")
            challenges = {}
            for challenge in response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_CHALLENGES]:
                challenges[challenge[conf.RCDPV2_RESPONSE_PARAM_NAME_NAME]] = challenge[
                    conf.RCDPV2_RESPONSE_PARAM_NAME_VALUE]
            response_names = response_payload.get(
                conf.RCDPV2_RESPONSE_PARAM_NAME_RESPONSE_NAMES, None)
            return challenges, response_names
        elif auth_status == conf.AUTH_EXPIRED:
            log("Password expired")
            return -1
        else:
            raise Exception(
                'Got {} trying to authenticate against service {}.'.format(auth_status, service))

    def change_password(self, old_password, new_password):
        request_params = {
            conf.RCDPV2_REQUEST_PARAM_NAME_OLD_PASSWORD: old_password,
            conf.RCDPV2_REQUEST_PARAM_NAME_NEW_PASSWORD: new_password
        }
        debug("Changing user password")
        self._request(conf.RCDPV2_REQUEST_CHANGE_PASSWORD, request_params)
        response_payload, _ = KeyTalkProtocol._parse_rcdp_response(
            self.conn, conf.RCDPV2_REQUEST_CHANGE_PASSWORD, conf.RCDPV2_RESPONSE_AUTH_RESULT)
        auth_status = response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_AUTH_STATUS]
        if auth_status == conf.AUTH_OK:
            log("Password successfully changed")
        else:
            raise Exception(
                'Got {} trying to change user password.'.format(auth_status))

    def get_last_messages(self):
        request_params = {}
        if LAST_MESSAGES_FROM_UTC is not None:
            request_params[
                conf.RCDPV2_REQUEST_PARAM_NAME_LAST_MESSAGES_FROM_UTC] = LAST_MESSAGES_FROM_UTC
        self._request(conf.RCDPV2_REQUEST_LAST_MESSAGES, request_params)
        response_payload, _ = KeyTalkProtocol._parse_rcdp_response(
            self.conn, conf.RCDPV2_REQUEST_LAST_MESSAGES, conf.RCDPV2_RESPONSE_LAST_MESSAGES)
        messages = response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_LAST_MESSAGES]
        if len(messages) > 0:
            log("Received {} user messages:\n{}".format(len(messages), pprint.pformat(messages)))
        return response_payload

    def get_cert(self, format, include_chain, out_of_band=False):
        request_params = {
            conf.RCDPV2_REQUEST_PARAM_NAME_CERT_FORMAT: format,
            conf.RCDPV2_REQUEST_PARAM_NAME_CERT_INCLUDE_CHAIN: include_chain,
            conf.RCDPV2_REQUEST_PARAM_NAME_CERT_OUT_OF_BAND: out_of_band,
        }

        self._request(conf.RCDPV2_REQUEST_CERT, request_params)
        response_payload, _ = KeyTalkProtocol._parse_rcdp_response(
            self.conn, conf.RCDPV2_REQUEST_CERT, conf.RCDPV2_RESPONSE_CERT)

        if out_of_band:
            cert_url_templ = response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_CERT_URL_TEMPL]
            cert_url = cert_url_templ.replace(
                "$(" + conf.CERT_DOWNLOAD_URL_HOST_PLACEHOLDER + ")", KEYTALK_SERVER)
            cert = KeyTalkProtocol._request_simple_url(cert_url)
            assert not KeyTalkProtocol._request_simple_url(
                cert_url), "the given certificate can only be downloaded once"
        else:
            cert = bytes(response_payload[conf.RCDPV2_RESPONSE_PARAM_NAME_CERT], 'utf-8')
            if format == conf.CERT_FORMAT_P12:
                cert = base64.b64decode(cert)

        cert_passphrase = self._get_cert_passphrase()
        log("Successfully received {} certificate {} chain".format(
            format, "with" if include_chain else "without"))
        cert_path, cert_pass_path = KeyTalkProtocol._save_cert(cert, cert_passphrase, format)
        return cert_path, cert_pass_path

    def reset_user_password(self, password):
        pass


def request_cert_with_password_authentication(cert_format, cert_with_chain):
    service = "CUST_PASSWD_INTERNAL_TESTUI"
    username = 'DemoUser'
    password = 'secret'

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert not KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "Non-CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, username, password)
    proto.authenticate(creds, service)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain)
    # close connection
    proto.eoc()


def request_out_of_band_cert_with_password_authentication(cert_format, cert_with_chain):
    service = "CUST_PASSWD_INTERNAL_TESTUI"
    username = 'DemoUser'
    password = 'secret'

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert not KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "Non-CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, username, password)
    proto.authenticate(creds, service)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain, out_of_band=True)
    # close connection
    proto.eoc()


def request_cert_with_password_and_pincode_authentication(cert_format, cert_with_chain):
    service = "CUST_PIN_PASSWD_INTERNAL_TESTUI"
    username = 'DemoUser'
    password = 'secret'
    pincode = '1234'

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert not KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "Non-CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(
        auth_requirements, username, password, pincode)
    proto.authenticate(creds, service)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain)
    # close connection
    proto.eoc()


def request_cert_with_challenge_response_authentication(cert_format, cert_with_chain):
    service = "CUST_CR_INTERNAL_TESTUI"
    username = 'DemoUser'

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, username)
    challenges, response_names = proto.authenticate(creds, service)
    creds = KeyTalkProtocol.calc_responses(username, challenges, response_names)
    proto.authenticate(creds)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain)
    # close connection
    proto.eoc()


def request_cert_with_radius_securid_authentication(cert_format, cert_with_chain):
    proto = KeyTalkProtocol()
    service = "CUST_PASSWD_RADIUS"
    username = 'SecuridNewUserPinUser'
    initial_tokencode = '666666'
    new_pin = '234567'
    new_tokencode = '777777'

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert not KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "Non-CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(
        auth_requirements, username, initial_tokencode)
    proto.authenticate(creds, service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, username, new_pin)
    proto.authenticate(creds, service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, username, new_pin)
    proto.authenticate(creds, service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, username, new_tokencode)
    proto.authenticate(creds, service)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain)
    # close connection
    proto.eoc()


def request_cert_with_radius_eap_aka_authentication(cert_format, cert_with_chain):
    proto = KeyTalkProtocol()
    service = "CUST_EAP_CR_RADIUS"

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, UMTS_USERNAME)
    challenges, response_names = proto.authenticate(creds, service)
    creds = KeyTalkProtocol.calc_responses(UMTS_USERNAME, challenges, response_names)
    proto.authenticate(creds)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain)
    # close connection
    proto.eoc()


def request_cert_with_radius_eap_sim_authentication(cert_format, cert_with_chain):
    proto = KeyTalkProtocol()
    service = "CUST_EAP_CR_RADIUS"

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, GSM_USERNAME)
    challenges, response_names = proto.authenticate(creds, service)
    creds = KeyTalkProtocol.calc_responses(GSM_USERNAME, challenges, response_names)
    challenges, response_names = proto.authenticate(creds)
    creds = KeyTalkProtocol.calc_responses(GSM_USERNAME, challenges, response_names)
    challenges, response_names = proto.authenticate(creds)
    creds = KeyTalkProtocol.calc_responses(GSM_USERNAME, challenges, response_names)
    proto.authenticate(creds)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain)
    # close connection
    proto.eoc()


def change_password_and_request_cert(cert_format, cert_with_chain):
    service = "CUST_PASSWD_AD"
    username = 'TestUser'
    old_password = 'Sioux2010'
    new_password = 'Sioux2011'

    proto = KeyTalkProtocol()
    # handshake
    proto.hello()
    proto.handshake()
    # authenticate
    auth_requirements = proto.get_auth_requirements(service)
    assert not KeyTalkProtocol.is_cr_authentication(
        auth_requirements), "Non-CR authentication is expected for service {}".format(service)
    creds = KeyTalkProtocol.request_auth_credentials(auth_requirements, username, old_password)
    password_validity_sec = proto.authenticate(creds, service)
    assert KeyTalkProtocol.is_password_expiring(
        password_validity_sec), "Password for user {} and service {} is not yet expiring (still valid for {} seconds)".format(username, service, password_validity_sec)
    proto.change_password(old_password, new_password)
    creds[conf.CRED_PASSWD] = new_password
    proto.authenticate(creds, service)
    # get service
    proto.get_last_messages()
    proto.get_cert(cert_format, cert_with_chain)
    # reset password back
    proto.change_password(new_password, old_password)
    creds[conf.CRED_PASSWD] = old_password
    proto.authenticate(creds, service)
    # close connection
    proto.eoc()


#
# Entry point
#
if __name__ == "__main__":
    for cert_format in conf.CERT_FORMAT_STRINGS:
        for cert_with_chain in [True, False]:
            request_cert_with_password_authentication(cert_format, cert_with_chain)
            request_cert_with_password_and_pincode_authentication(cert_format, cert_with_chain)
            request_cert_with_challenge_response_authentication(cert_format, cert_with_chain)
            request_cert_with_radius_securid_authentication(cert_format, cert_with_chain)
            request_cert_with_radius_eap_aka_authentication(cert_format, cert_with_chain)
            request_cert_with_radius_eap_sim_authentication(cert_format, cert_with_chain)
            request_out_of_band_cert_with_password_authentication(cert_format, cert_with_chain)
            change_password_and_request_cert(cert_format, cert_with_chain)
