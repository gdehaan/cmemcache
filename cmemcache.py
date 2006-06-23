#!/usr/bin/env python
#
# $Id$
#

"""
The extension only supports string values. This module defines another client on top
of the extension to support any picklable object. Logic is a straight copy from
memcache.py.
"""

__version__ = "$Revision$"
__author__ = "$Author$"

import types
try:
    import cPickle as pickle
except ImportError:
    import pickle

from _cmemcache import Client

#-----------------------------------------------------------------------------------------
#
class PickleClient(Client):
    """
    Use memcached flags parameter to set/add/replace to handle any python class as
    the cache value. Also does int, long conversion to/from string.
    """

    _FLAG_PICKLE  = 1<<0
    _FLAG_INTEGER = 1<<1
    _FLAG_LONG    = 1<<2

    @staticmethod
    def _convert(val):
        """
        Convert val to str, flags tuple.
        """
        if isinstance(val, types.StringTypes):
            flags = 0
        elif isinstance(val, int):
            flags = PickleClient._FLAG_INTEGER
            val = "%d" % val
        elif isinstance(val, long):
            flags = PickleClient._FLAG_LONG
            val = "%d" % val
        else:
            flags = PickleClient._FLAG_PICKLE
            val = pickle.dumps(val, 2)
        return (val, flags)

    def set(self, key, val, time=0):
        """
        Set key with some object val.
        """
        val, flags = self._convert(val)
        return Client.set(self, key, val, time, flags)

    def add(self, key, val, time=0):
        """
        Add key with some object val.
        """
        val, flags = self._convert(val)
        return Client.add(self, key, val, time, flags)

    def replace(self, key, val, time=0):
        """
        Replace key with some object val.
        """
        val, flags = self._convert(val)
        return Client.replace(self, key, val, time, flags)

    def get(self, key):
        """
        Get key value and convert it to the right object if appropriate.
        """
        val = Client.getflags(self, key)
        if val:
            buf, flags = val
            if flags == 0:
                val = buf
            elif flags & PickleClient._FLAG_INTEGER:
                val = int(buf)
            elif flags & PickleClient._FLAG_LONG:
                val = long(buf)
            elif flags & PickleClient._FLAG_PICKLE:
                try:
                    val = pickle.loads(buf)
                except:
                    self.debuglog('Pickle error...\n')
                    val = None
            else:
                self.debuglog("unknown flags on get: %x\n" % flags)

        return val
        
