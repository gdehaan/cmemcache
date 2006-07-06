#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# $Id$
#

"""
Test for cmemcache module.
"""

__version__ = "$Revision$"
__author__ = "$Author$"

import os, signal, socket, subprocess, unittest

#-----------------------------------------------------------------------------------------
#
def to_s(val):
    """
    Convert val to string.
    """
    if not isinstance(val, str):
        return "%s (%s)" % (val, type(val))
    return val

#-----------------------------------------------------------------------------------------
#
def test_setget(mc, key, val, checkf):
    """
    test set and get in one go
    """
    mc.set(key, val)
    newval = mc.get(key)
    checkf(val, newval)
    
#-------------------------------------------------------------------------------
#
class TestCmemcache( unittest.TestCase ):

    servers = ["127.0.0.1:11211"]
    servers_unknown = ["127.0.0.1:52345"]
    servers_weighted = [("127.0.0.1:11211", 2)]

    def _test_cmemcache(self, mcm):
        """
        Test cmemcache specifics.
        """
        mc = mcm.StringClient(self.servers)
        mc.set('blo', 'blu', 0, 12)
        self.failUnlessEqual(mc.get('blo'), 'blu')
        self.failUnlessEqual(mc.getflags('blo'), ('blu', 12))

        # try weird server formats
        # number is not a server
        self.failUnlessRaises(TypeError, lambda: mc.set_servers([12]))
        # forget port
        self.failUnlessRaises(TypeError, lambda: mc.set_servers(['12']))
        
    def _test_sgra(self, mc, val, repval, norepval, ok):
        """
        Test set, get, replace, add api.
        """
        self.failUnlessEqual(mc.set('blo', val), ok)
        self.failUnlessEqual(mc.get('blo'), val)
        mc.replace('blo', repval)
        self.failUnlessEqual(mc.get('blo'), repval)
        mc.add('blo', norepval)
        self.failUnlessEqual(mc.get('blo'), repval)

        mc.delete('blo')
        self.failUnlessEqual(mc.get('blo'), None)
        mc.replace('blo', norepval)
        self.failUnlessEqual(mc.get('blo'), None)
        mc.add('blo', repval)
        self.failUnlessEqual(mc.get('blo'), repval)

    def _test_base(self, mcm, mc, ok):
        """
        The base test, uses string values only.

        The return codes are not compatible between memcache and cmemcache.  memcache
        return 1 for any reply from memcached, and cmemcache returns the return code
        returned by memcached.

        Actually the return codes from libmemcache for replace and add do not seem to be
        logical either. So ignore them and tests through get() if the appropriate action
        was done.

        """

        print 'testing', mc, '\n\tfrom', mcm

        self._test_sgra(mc, 'blu', 'replace', 'will not be set', ok)

        mc.delete('blo')
        self.failUnlessEqual(mc.get('blo'), None)
        
        mc.set('number', '5')
        self.failUnlessEqual(mc.get('number'), '5')
        self.failUnlessEqual(mc.incr('number', 3), 8)
        self.failUnlessEqual(mc.decr('number', 2), 6)
        self.failUnlessEqual(mc.get('number'), '6')

        mc.set('blo', 'bli')
        self.failUnlessEqual(mc.get('blo'), 'bli')
        d = mc.get_multi(['blo', 'number', 'doesnotexist'])
        self.failUnlessEqual(d, {'blo':'bli', 'number':'6'})

        # make sure zero delimitation characters are ignored in values.
        test_setget(mc, 'blabla', 'bli\000bli', self.failUnlessEqual)

        # get stats
        stats = mc.get_stats()
        self.failUnlessEqual(len(stats), 1)
        self.assert_(self.servers[0] in stats[0][0])
        self.assert_('total_items' in stats[0][1])
        self.assert_('bytes_read' in stats[0][1])
        self.assert_('bytes_written' in stats[0][1])
        
        # set_servers to none
        mc.set_servers([])
        try:
            # memcache does not support the 0 server case
            mc.set('bli', 'bla')
        except ZeroDivisionError:
            pass
        else:
            self.failUnlessEqual(mc.get('bli'), None)

        # set unknown server
        # mc.set_servers(self.servers_unknown)
        # test_setget(mc, 'bla', 'bli', self.failIfEqual)

        # set servers with weight syntax
        mc.set_servers(self.servers_weighted)
        test_setget(mc, 'bla', 'bli', self.failUnlessEqual)
        test_setget(mc, 'blo', 'blu', self.failUnlessEqual)

        # set servers again
        mc.set_servers(self.servers)
        test_setget(mc, 'bla', 'bli', self.failUnlessEqual)
        test_setget(mc, 'blo', 'blu', self.failUnlessEqual)

        # test unicode
        test_setget(mc, 'blo', '© 2006', self.failUnlessEqual)

        # flush_all
        # fixme: how to test this?
        # fixme: after doing flush_all() one can not start new Client(), do not know why
        # since I know no good way to test it we ignore it for now
        # mc.flush_all()

        mc.disconnect_all()

    def _test_client(self, mcm, ok):
        """
        Test Client, only need to test the set, get, add, replace, rest is
        implemented by test_memcache().
        """
        mc = mcm.Client(self.servers)

        self._test_sgra(mc, 'blu', 'replace', 'will not be set', ok)

        val = {'bla':'bli', 'blo':12}
        repval = {'bla':'blo', 'blo':12}
        norepval = {'blo':12}
        self._test_sgra(mc, val, repval, norepval, ok)

    def test_memcache(self):
        # quick check if memcached is running
        ip, port = self.servers[0].split(':')
        print 'ip', ip, 'port', port
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        memcached = None
        try:
            s.connect((ip, int(port)))
        except socket.error, e:
            # not running, start one
            memcached = subprocess.Popen("memcached -m 10", shell=True)
            print 'memcached not running, starting one (pid %d)' % (memcached.pid,)
            # give it some time to start
            import time
            time.sleep(0.5)
        s.close()

        # use memcache as the reference
        try:
            import memcache
        except ImportError:
            pass
        else:
            self._test_base(memcache, memcache.Client(self.servers), ok=1)
            self._test_client(memcache, ok=1)
        
        # test extension
        import cmemcache
        self._test_cmemcache(cmemcache)
        self._test_base(cmemcache, cmemcache.StringClient(self.servers), ok=0)
        self._test_base(cmemcache, cmemcache.Client(self.servers), ok=0)
        self._test_client(cmemcache, ok=0)

        # if we created memcached for our test, then shut it down
        if memcached:
            os.kill(memcached.pid, signal.SIGINT)

if __name__ == '__main__':
    unittest.main()
