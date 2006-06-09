#!/usr/bin/env python
#
# $Id$
#

"""
Test for cmemcache module.
"""

__version__ = "$Revision$"
__author__ = "$Author$"

#-------------------------------------------------------------------------------
#
def main():
    import optparse
    parser = optparse.OptionParser(__doc__.strip())
    opts, args = parser.parse_args()

    servers = ["127.0.0.1:11211"]

    def to_s(val):
        """
        Convert val to string.
        """
        if not isinstance(val, str):
            return "%s (%s)" % (val, type(val))
        return val

    def test_setget(mc, key, val):
        """
        test set and get in one go
        """
        print "Testing set/get {'%s': %s} ..." % (to_s(key), to_s(val)),
        mc.set(key, val)
        newval = mc.get(key)
        if newval == val:
            print "OK"
            return 1
        else:
            print "FAIL"
            return 0
    
    def test(mcm, ok):
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
        mc = mcm.Client(servers,debug=1)

        print 'blo "%s"' % mc.get('blo')
        assert(mc.set('blo', 'blu') == ok)
        assert(mc.get('blo'), 'blu')
        mc.replace('blo', 'replace')
        assert(mc.get('blo'), 'replace')
        mc.add('blo', 'will be NOT set')
        assert(mc.get('blo'), 'replace')

        mc.delete('blo')
        assert(mc.get('blo'), '')
        mc.replace('blo', 'will NOT be set')
        assert(mc.get('blo'), '')
        mc.add('blo', 'will be set')
        assert(mc.get('blo'), 'will be set')
        
        assert(mc.delete('blo') != 0)
        assert(mc.get('blo'), None)
        
        mc.set('number', '5')
        assert(mc.incr('number', 3), 8)
        assert(mc.decr('number', 2), 6)

        mc.set('blo', 'bli')
        d = mc.get_multi(['blo', 'number', 'doesnotexist'])
        print 'd', d
        assert(d['blo'] == 'bli')
        assert(d['number'] == '6')
        assert(not d.has_key('doesnotexist'))
        
        # make sure zero delimitation characters are ignored in values.
        test_setget(mc, 'blabla', 'bli\000bli')
        
        # set_servers to none
        mc.set_servers([])
        try:
            # memcache does not support to 0 server case
            mc.set('bli', 'bla')
        except ZeroDivisionError:
            pass
        else:
            assert(mc.get('bli'), '')
        
        # set servers again
        mc.set_servers(servers)
        test_setget(mc, 'bla', 'bli')
        test_setget(mc, 'blo', 'blu')

        # flush_all
        # fixme: how to test this?
        mc.flush_all()

    # use memcache as the reference
    import memcache
    test(memcache, ok=1)

    # now test c- module
    import cmemcache
    test(cmemcache, ok=0)

if __name__ == '__main__':
    main()
