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
    import cmemcache
    mc = cmemcache.Client(servers,debug=1)

    mc.set('blo', 'blu')
    assert(mc.get('blo'), 'blu')
    assert(mc.get('blof'), None)
    mc.delete('blo')
    assert(mc.get('blo'), None)

    def to_s(val):
        if not isinstance(val, str):
            return "%s (%s)" % (val, type(val))
        return val
    def test_setget(key, val):
        print "Testing set/get {'%s': %s} ..." % (to_s(key), to_s(val)),
        mc.set(key, val)
        newval = mc.get(key)
        if newval == val:
            print "OK"
            return 1
        else:
            print "FAIL"
            return 0

    test_setget('bla', 'bli')

    # make sure zero delimitation characters are ignored in values.
    test_setget('blabla', 'bli\000bli')

if __name__ == '__main__':
    main()
