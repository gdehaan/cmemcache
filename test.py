#!/usr/bin/env python
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

    def _test(self, mcm, ok):
        """
        The test.

        The return codes are not compatible between memcache and cmemcache.  memcache
        return 1 for any reply from memcached, and cmemcache returns the return code
        returned by memcached.

        Actually the return codes from libmemcache for replace and add do not seem to be
        logical either. So ignore them and tests through get() if the appropriate action
        was done.

        """

        print 'testing', mcm
        # setup
        mc = mcm.Client(self.servers, debug=1)

        self.assert_(mc.set('blo', 'blu') == ok)
        self.failUnlessEqual(mc.get('blo'), 'blu')
        mc.replace('blo', 'replace')
        self.failUnlessEqual(mc.get('blo'), 'replace')
        mc.add('blo', 'will be NOT set')
        self.failUnlessEqual(mc.get('blo'), 'replace')

        mc.delete('blo')
        self.failUnlessEqual(mc.get('blo'), None)
        mc.replace('blo', 'will NOT be set')
        self.failUnlessEqual(mc.get('blo'), None)
        mc.add('blo', 'will be set')
        self.failUnlessEqual(mc.get('blo'), 'will be set')
        
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
        
        # set_servers to none
        mc.set_servers([])
        try:
            # memcache does not support the 0 server case
            mc.set('bli', 'bla')
        except ZeroDivisionError:
            pass
        else:
            self.failUnlessEqual(mc.get('bli'), None)

        # try weird server formats
        # number is not a server
        self.failUnlessRaises(TypeError, lambda: mc.set_servers([12]))
        # forget port
        self.failUnlessRaises(TypeError, lambda: mc.set_servers(['12']))
        
        # set servers again
        mc.set_servers(self.servers)
        test_setget(mc, 'bla', 'bli', self.failUnlessEqual)
        test_setget(mc, 'blo', 'blu', self.failUnlessEqual)

        # flush_all
        # fixme: how to test this?
        mc.flush_all()

        mc.disconnect_all()

    def test_memcache( self ):
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
        s.close()

        # fixme: it seems like one can only start one Client from python (or maybe the
        # process?). I was trying to run the test on memcache and then on cmemcache but
        # then the first mc.get() fails. I thought at first it was my cmemcache
        # implementation, but running the test twice on memcache fails as well!

        # use memcache as the reference
        # import memcache
        # self._test(memcache, ok=1)
        # self._test(memcache, ok=1)
        
        # test c- module
        import cmemcache
        self._test(cmemcache, ok=0)

        # if we created memcached for our test, then shut it down
        if memcached:
            os.kill(memcached.pid, signal.SIGINT)

if __name__ == '__main__':
    unittest.main()
