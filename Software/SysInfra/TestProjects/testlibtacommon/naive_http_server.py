#!/usr/bin/env python
# -*- coding: UTF-8 -*-

import sys
from CGIHTTPServer import CGIHTTPRequestHandler
import BaseHTTPServer
import os


class NaiveHTTPRequestHandler (CGIHTTPRequestHandler):
    def do_HEAD(self):
        try:
            CGIHTTPRequestHandler.do_HEAD(self)
        except:
            pass
    def do_GET(self):
        try:
            CGIHTTPRequestHandler.do_GET(self)
        except:
            pass  


class NaiveHttpServerBase(BaseHTTPServer.HTTPServer):
    allow_reuse_address = True

    def __init__(self, *args, **kw):
        self._stop_request = False
        BaseHTTPServer.HTTPServer.__init__(self, *args, **kw)

    def serve_forever(self):
        while not self._stop_request:
            self.handle_request()
               
    def stop(self):
        self.server_close()
        self._stop_request = True

class NaiveIpv4HttpServer(NaiveHttpServerBase):
    pass

class NaiveIpv6HttpServer(NaiveHttpServerBase):
    import socket
    if socket.has_ipv6:
        address_family = socket.AF_INET6
    else:
        raise Exception("IPv6 is not supported on this platform")


class NaiveHttpServer:
    def __init__(self, host, port, ipv6 = False):
        self._host = host
        self._port = int(port)
        self._ipv6 = bool(int(ipv6))
        if self._ipv6:
            self._httpd = NaiveIpv6HttpServer((self._host, self._port), NaiveHTTPRequestHandler)
            f = open("naive_http_server_ipv6.pid", "w")
            print >> f, os.getpid()
            f.close()
        else:
            self._httpd = NaiveIpv4HttpServer((self._host, self._port), NaiveHTTPRequestHandler)
            f = open("naive_http_server_ipv4.pid", "w")
            print >> f, os.getpid()
            f.close()

    def start(self):
        print "Start listening %s:%d, ipv6: %s" % (self._host, self._port, self._ipv6)
        self._httpd.serve_forever()
        
    def stop(self):
        self._httpd.stop()


def usage():
    print >> sys.stderr, "Usage: python naive_http_server <listen-address> <listen-port> [<is-ipv6> = 0]"  

def main(argv=None):
    if argv is None:
        argv = sys.argv
    if  len(argv) < 3 or len(argv) > 4:
        usage()
        return 2
    svr = NaiveHttpServer(*argv[1:])
    svr.start()



if __name__ == '__main__':
    sys.exit(main()) 
