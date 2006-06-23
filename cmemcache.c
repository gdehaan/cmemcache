/*
  $Id$

  Python extension for the C interface to memcached, using libmemcache library.
*/

#include <Python.h>
#include "memcache.h"

typedef struct 
{
    PyObject_HEAD
    struct memcache* mc;
    int debug;
} CmemcacheObject;

#ifdef NDEBUG
#define debug(args)
#define debug_def(args)
#else
#define debug(args) printf args
#define debug_def(args) args
#endif

//----------------------------------------------------------------------------------------
//
static int
do_set_servers(CmemcacheObject* self, PyObject* servers)
{
    debug(("do_set_servers\n"));
    
    if (!PySequence_Check(servers))
    {
        PyErr_BadArgument();
        return -1;
    }

    int error = 0;
    
    /* there seems to be no way to remove servers, so get rid of memcache all together */
    if (self->mc)
    {
        mc_free(self->mc);
        self->mc = NULL;
    }
    assert(self->mc == NULL);

    /* create new instance */
    self->mc = mc_new();
    if (self->mc == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }
    
    /* add servers, allow any sequence of strings */
    const int size = PySequence_Size(servers);
    int i;
    for (i = 0; i < size && error == 0; ++i)
    {
        PyObject* item = PySequence_GetItem(servers, i);
        if (item)
        {
            PyObject* name = NULL;
            int weight = 1;
            
            if (PyString_Check(item))
            {
                name = item;
            }
            else if (PyTuple_Check(item))
            {
                error = ! PyArg_ParseTuple(item, "Oi", &name, &weight);
            }
            if (name)    
            {
                const char* cserver = PyString_AsString(name);
                assert(cserver);
                debug(("cserver %s weight %d\n", cserver, weight));
            
                /* mc_server_add4 is not happy without ':' (it segfaults!) so check */
                if (strstr(cserver, ":") == NULL)
                {
                    PyErr_Format(PyExc_TypeError,
                                 "expected \"server:port\" but \"%s\" found", cserver);
                    error = 1;
                }
                else
                {
                    int i;
                    for (i = 0; i < weight; ++i)
                    {
                        debug_def(int retval =) mc_server_add4(self->mc, cserver);
                        debug(("retval %d\n", retval));
                    }
                }
            }
            else
            {
                PyErr_BadArgument();
                error = 1;
            }
            Py_DECREF(item);
        }
    }
    if (error)
    {
        mc_free(self->mc);
        self->mc = NULL;
        return -1;
    }
    return 0;
}

//----------------------------------------------------------------------------------------
//
static int
cmemcache_init(CmemcacheObject* self, PyObject* args, PyObject* kwds)
{
    PyObject* servers=NULL;
    char debug = 0;

    if (! PyArg_ParseTuple(args, "O|b", &servers, &debug))
        return -1; 

    self->debug = debug;
    return do_set_servers(self, servers);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_set_servers(PyObject* pyself, PyObject* servers)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    if (do_set_servers(self, servers) != -1)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }
    /* fixme: what to return in the error case, shouldn't an exception have been raised? */
    return NULL;
}

//----------------------------------------------------------------------------------------
//
static void
cmemcache_dealloc(CmemcacheObject* self)
{
    debug(("cmemcache_dealloc\n"));
    
    if (self->mc)
    {
        mc_free(self->mc);
        self->mc = NULL;
    }
}

enum StoreType
{
    SET,
    ADD,
    REPLACE
};

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_store(PyObject* pyself, PyObject* args, enum StoreType storeType)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    assert(self->mc);
    
    char* key = NULL;
    int keylen = 0;
    const char* value = NULL;
    int valuelen = 0;
    int time = 0;
    int flags = 0;
    
    if (! PyArg_ParseTuple(args, "s#s#|ii",
                           &key, &keylen, &value, &valuelen, &time, &flags))
        return NULL;
    
    debug(("cmemcache_store %d %s '%s' time %d flags %d\n",
           storeType, key, value, time, flags));
    int retval = 0;
    switch(storeType)
    {
        case SET:
            retval = mc_set(self->mc, key, keylen, value, valuelen, time, flags);
            break;
        case ADD:
            retval = mc_add(self->mc, key, keylen, value, valuelen, time, flags);
            break;
        case REPLACE:
            retval = mc_replace(self->mc, key, keylen, value, valuelen, time, flags);
            break;
    }
    debug(("retval = %d\n", retval));
    return PyInt_FromLong(retval);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_set(PyObject* pyself, PyObject* args)
{
    return cmemcache_store(pyself, args, SET);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_add(PyObject* pyself, PyObject* args)
{
    return cmemcache_store(pyself, args, ADD);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_replace(PyObject* pyself, PyObject* args)
{
    return cmemcache_store(pyself, args, REPLACE);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_get_imp(PyObject* pyself, PyObject* args, int retFlags)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    assert(self->mc);
    
    char* key = NULL;
    int keylen = 0;

    if (! PyArg_ParseTuple(args, "s#", &key, &keylen))
    {
        debug(("bad arguments\n"));
        return NULL;
    }
    debug(("cmemcache_get_imp %s len %d\n", key, keylen));
    
    struct memcache_req *req;
    struct memcache_res *res;
    req = mc_req_new();
    res = mc_req_add(req, key, keylen);
    mc_res_free_on_delete(res, 1);
    mc_get(self->mc, req);
    debug(("attempt %d found %d res %d '%s'\n",
           mc_res_attempted(res), mc_res_found(res), res->size, (char*)res->val));
    PyObject* retval;
    if (mc_res_found(res))
    {
        if (retFlags)
        {
            retval = Py_BuildValue("s#i", res->val, res->size, (int)res->flags);
        }
        else
        {
            retval = PyString_FromStringAndSize(res->val, res->size);
        }
    }
    else
    {
        Py_INCREF(Py_None);
        retval = Py_None;
    }
    mc_req_free(req);
    return retval;
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_get(PyObject* pyself, PyObject* args)
{
    return cmemcache_get_imp(pyself, args, 0);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_getflags(PyObject* pyself, PyObject* args)
{
    return cmemcache_get_imp(pyself, args, 1);
}

//----------------------------------------------------------------------------------------
//
static void get_multi_cb(MCM_CALLBACK_FUNC)
{
    debug(("get_multi_cb %p\n", MCM_CALLBACK_PTR));
    PyObject* dict = (PyObject*)MCM_CALLBACK_PTR;
    struct memcache_res *res = MCM_CALLBACK_RES;
    debug(("add %s %s attempted %d found %d\n",
           res->key, (char*)res->val, mc_res_attempted(res), mc_res_found(res)));
    if(mc_res_found(res))
    {
        debug(("res found, add it\n"));
        PyObject* key = PyString_FromStringAndSize(res->key, res->len);
        PyObject* val = PyString_FromStringAndSize(res->val, res->size);
        PyDict_SetItem(dict, key, val);
    }
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_get_multi(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_get_multi\n"));

    assert(self->mc);
    
    PyObject* keys = NULL;

    if (! PyArg_ParseTuple(args, "O", &keys))
        return NULL;
    debug(("cmemcache_get_multi\n"));
    
    struct memcache_req *req;
    struct memcache_res *res;
    req = mc_req_new();
    const int size = PySequence_Size(keys);
    int i;
    int error = 0;
    PyObject* dict = PyDict_New();
    for (i = 0; i < size && error == 0; ++i)
    {
        PyObject* key = NULL;
        key = PySequence_GetItem(keys, i);
        if (PyString_Check(key))
        {
            char* ckey = PyString_AsString(key);
            if (ckey)
            {
                debug(("key \"%s\" len %d\n", ckey, PyString_Size(key)));
                res = mc_req_add(req, ckey, PyString_Size(key));
                debug(("res %p val %p size %d\n", res, res->val, res->size));
                mc_res_free_on_delete(res, 1);
                mc_res_register_fetch_cb(req, res, get_multi_cb, dict);
            }
            else
            {
                PyErr_BadArgument();
                error = 1;
            }
        }
        else
        {
            debug(("not a string\n"));
            PyErr_BadArgument();
            error = 1;
        }
        Py_DECREF(key);
    }
    if (error)
    {
        debug(("error\n"));
        PyDict_Clear(dict);
    }
    else
    {
        debug(("before mc_get\n"));
        mc_get(self->mc, req);
        debug(("after mc_get\n"));
    }
    mc_req_free(req);

    return dict;
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_delete(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_delete\n"));

    assert(self->mc);
    
    char* key = NULL;
    int keylen = 0;
    int time = 0;

    if (! PyArg_ParseTuple(args, "s#|i", &key, &keylen, &time))
        return NULL;
    debug(("cmemcache_delete %s time %d\n", key, time));

    int retval = mc_delete(self->mc, key, keylen, time);
    debug(("retval = %d\n", retval));
    return PyInt_FromLong(retval);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_decr(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_decr\n"));

    assert(self->mc);
    
    char* key = NULL;
    int keylen = 0;
    int val = 0;

    if (! PyArg_ParseTuple(args, "s#i", &key, &keylen, &val))
        return NULL;
    debug(("cmemcache_decr %s val %d\n", key, val));

    int newval;
    newval = mc_decr(self->mc, key, keylen, val);
    debug(("newval %d\n", newval));
    return PyInt_FromLong(newval);
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_incr(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_incr\n"));

    assert(self->mc);
    
    char* key = NULL;
    int keylen = 0;
    int val = 0;

    if (! PyArg_ParseTuple(args, "s#i", &key, &keylen, &val))
        return NULL;
    debug(("cmemcache_incr %s val %d\n", key, val));

    int newval;
    newval = mc_incr(self->mc, key, keylen, val);
    debug(("newval %d\n", newval));
    PyObject* retval;
    retval = PyInt_FromLong(newval);
    return retval;
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_get_stats(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_get_stats\n"));

    assert(self->mc);
    PyObject* retval = PyList_New(0);

    /* loop copied from mcm_server_disconnect_all, but is there some supported way of
       going through all the servers? */
    struct memcache_server *ms;
    for (ms = self->mc->server_list.tqh_first; ms != NULL; ms = ms->entries.tqe_next)
    {
        struct memcache_server_stats* stats;
        stats = mc_server_stats(self->mc, ms);
        if (stats != NULL)
        {
            char buffer[128+1];
            snprintf(buffer, 128, "%s:%s", ms->hostname, ms->port);
            PyObject* name = PyString_FromString(buffer);
            PyObject* dict = PyDict_New();

#define SET_SNPRINTF(d, key, fmt, val)           \
            snprintf(buffer, 128, fmt, val);   \
            PyDict_SetItem(d, PyString_FromString(key), PyString_FromString(buffer))
            
#define SET_TIME(d, key, val) SET_SNPRINTF(d, key, "%ld", val)
#define SET_ITEMU32(d, key, val) SET_SNPRINTF(d, key, "%u", val)
#define SET_ITEMU64(d, key, val) SET_SNPRINTF(d, key, "%llu", val)
#define SET_TIMEVAL(d, key, val)                                \
            SET_SNPRINTF(d, key, "%lf", val.tv_sec + val.tv_usec * 1.0e-9)
                
            SET_ITEMU32(dict, "pid", stats->pid);
            SET_TIME(dict, "uptime", stats->uptime);
            SET_TIME(dict, "time", stats->time);
            PyDict_SetItem(dict, PyString_FromString("version"),
                           PyString_FromString(stats->version));
            SET_TIMEVAL(dict, "rusage_user", stats->rusage_user);
            SET_TIMEVAL(dict, "rusage_system", stats->rusage_system);
            SET_ITEMU32(dict, "curr_items", stats->curr_items);
            SET_ITEMU64(dict, "total_items", stats->total_items);
            SET_ITEMU64(dict, "bytes", stats->bytes);
            SET_ITEMU32(dict, "curr_connections", stats->curr_connections);
            SET_ITEMU64(dict, "total_connections", stats->total_connections);
            SET_ITEMU32(dict, "connection_structures", stats->connection_structures);
            SET_ITEMU64(dict, "cmd_get", stats->cmd_get);
#ifdef SEAN_HACKS
            SET_ITEMU64(dict, "cmd_refresh", stats->cmd_refresh);
#endif
            SET_ITEMU64(dict, "cmd_set", stats->cmd_set);
            SET_ITEMU64(dict, "get_hits", stats->get_hits);
            SET_ITEMU64(dict, "get_misses", stats->get_misses);
#ifdef SEAN_HACKS
            SET_ITEMU64(dict, "refresh_hits", stats->refresh_hits);
            SET_ITEMU64(dict, "refresh_misses", stats->refresh_misses);
#endif
            SET_ITEMU64(dict, "bytes_read", stats->bytes_read);
            SET_ITEMU64(dict, "bytes_written", stats->bytes_written);
            SET_ITEMU64(dict, "limit_maxbytes", stats->limit_maxbytes);

            PyObject* tuple = PyTuple_New(2);
            PyTuple_SetItem(tuple, 0, name);
            PyTuple_SetItem(tuple, 1, dict);
            PyList_Append(retval, tuple);
            
            mc_server_stats_free(stats);
        }
    }
    
    return retval;
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_flush_all(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_flush_all\n"));

    assert(self->mc);
    debug_def(int retval =) mc_flush_all(self->mc);
    debug(("retval = %d\n", retval));
    
    Py_INCREF(Py_None);
    return Py_None;
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_disconnect_all(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_disconnect_all\n"));

    assert(self->mc);
    mc_server_disconnect_all(self->mc);
    
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef cmemcache_methods[] = {
    {
        "set_servers", cmemcache_set_servers, METH_O,
        "Client.set_servers(servers) -- set memcached servers"
    },
    {
        "set", cmemcache_set, METH_VARARGS,
        "Client.set(key, value, time=0, flags=0) -- Unconditionally sets a key to a given value in the memcache.\n\n@return: Nonzero on success.\n@rtype: int\n"
    },
    {
        "add", cmemcache_add, METH_VARARGS,
        "add(key, value, time=0, flags=0) -- Add new key with value.\n\nLike L{set}, but only stores in memcache if the key doesn't already exist."
    },
    {
        "replace", cmemcache_replace, METH_VARARGS,
        "replace(key, value, time=0, flags=0) -- replace existing key with value.\n\nLike L{set}, but only stores in memcache if the key already exists.\nThe opposite of L{add}."
    },
    {
        "get", cmemcache_get, METH_VARARGS,
        "get(key) -- Retrieves a key from the memcache.\n\n@return: The value or None."
    },
    {
        "getflags", cmemcache_getflags, METH_VARARGS,
        "getflags(key) -- Retrieves a key from the memcache.\n\n@return: The (value,flags) or None."
    },
    {
        "get_multi", cmemcache_get_multi, METH_VARARGS,
        "get_multi(keys) --\n"
        "Retrieves multiple keys from the memcache doing just one query.\n"
        ">>> success = mc.set(\"foo\", \"bar\")\n"
        ">>> success = mc.set(\"baz\", 42)\n"
        ">>> mc.get_multi([\"foo\", \"baz\", \"foobar\"]) == {\"foo\": \"bar\", \"baz\": 42}\n"
        "\n"
        "This method is recommended over regular L{get} as it lowers the number of\n"
        "total packets flying around your network, reducing total latency, since\n"
        "your app doesn't have to wait for each round-trip of L{get} before sending\n"
        "the next one.\n"
        "\n"
        "@param keys: An array of keys.\n"
        "@return:  A dictionary of key/value pairs that were available.\n"
    },
    {
        "delete", cmemcache_delete, METH_VARARGS,
        "delete(key, time=0) -- Deletes a key from the memcache.\n\n@return: Nonzero on success.\n@rtype: int"
    },
    {
        "decr", cmemcache_decr, METH_VARARGS,
        "decr(key, val) -- decrement key's value with val, returns new value"
    },
    {
        "incr", cmemcache_incr, METH_VARARGS,
        "incr(key, val) -- increment key's value with val, returns new value"
    },
    {
        "get_stats", cmemcache_get_stats, METH_NOARGS,
        "get_stats() -- Get statistics from all servers.\n"
        "@return: A list of tuples ( server_identifier, stats_dictionary ).\n"
        "The dictionary contains a number of name/value pairs specifying\n"
        "the name of the status field and the string value associated with\n"
        "it.  The values are not converted from strings."
    },
    {
        "flush_all", cmemcache_flush_all, METH_NOARGS,
        "flush_all() -- flush all keys on all servers"
    },
    {
        "disconnect_all", cmemcache_disconnect_all, METH_NOARGS,
        "disconnect_all() -- disconnect all servers"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject cmemcache_CmemcacheType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "Client",                  /*tp_name*/
    sizeof(CmemcacheObject),   /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)cmemcache_dealloc,         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "Client object",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    cmemcache_methods,         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)cmemcache_init,  /* tp_init */
    0,                         /* tp_alloc */
    PyType_GenericNew,         /* tp_new */
};

static PyMethodDef cmemcache_module_methods[] = {
    {NULL}  /* Sentinel */
};

#ifndef PyMODINIT_FUNC	/* declarations for DLL import/export */
#define PyMODINIT_FUNC void
#endif
PyMODINIT_FUNC
init_cmemcache(void) 
{
    PyObject* m;

    debug(("init_cmemcache\n"));
    
#ifdef NDEBUG
    /* turn off some message/errors */
    debug(("error filter %x\n", mc_err_filter_get()));
    mc_err_filter_add(MCM_ERR_LVL_INFO);
    mc_err_filter_add(MCM_ERR_LVL_NOTICE);
    mc_err_filter_add(MCM_ERR_LVL_WARN);
    debug(("error filter %x\n", mc_err_filter_get()));
#endif
    
    cmemcache_CmemcacheType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&cmemcache_CmemcacheType) < 0)
        return;

    m = Py_InitModule3("_cmemcache", cmemcache_module_methods,
                       "Fast client to memcached.");

    Py_INCREF(&cmemcache_CmemcacheType);
    PyModule_AddObject(m, "Client", (PyObject *)&cmemcache_CmemcacheType);
}

/*
  Local Variables: ***
  compile-command: "cd .; python setup.py build_ext -i && python test.py" ***
  End: ***
*/  
