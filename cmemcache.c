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

#undef DEBUG
#ifdef DEBUG
#define debug(args) printf args
#else
#define debug(args)
#endif

//----------------------------------------------------------------------------------------
//
static int
cmemcache_init(CmemcacheObject* self, PyObject* args, PyObject* kwds)
{
    PyObject* servers=NULL;
    char debug = 0;

    if (! PyArg_ParseTuple(args, "O|b", &servers, debug))
        return -1; 

    self->debug = debug;
    
    if (!PySequence_Check(servers)) {
        PyErr_BadArgument();
        return -1;
    }

    int error = 0;
    assert(self->mc == NULL);
    self->mc = mc_new();
    if (self->mc == NULL)
    {
        // fixme: raise a different exception.
        PyErr_BadArgument();
        return -1;
    }
    const int size = PySequence_Size(servers);
    int i;
    for (i = 0; i < size; ++i)
    {
        PyObject* server = NULL;
        server = PySequence_GetItem(servers, i);
        if (!PyString_Check(server)) {
            PyErr_BadArgument();
            return -1;
        }
        const char* cserver = PyString_AsString(server);
        if (cserver)
        {
            debug(("cserver %s\n", cserver));
            mc_server_add4(self->mc, cserver);
        }
        else
        {
            PyErr_BadArgument();
            error = 1;
        }
    }
    if (error)
    {
        mc_free(self->mc);
        self->mc = NULL;
    }
    return 0;
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
    }
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_set(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_set\n"));

    assert(self->mc);
    
    char* key = NULL;
    int keylen = 0;
    const char* value = NULL;
    int valuelen = 0;
    int time = 0;
    
    if (! PyArg_ParseTuple(args, "s#s#|d", &key, &keylen, &value, &valuelen, &time))
        return NULL;
    debug(("cmemcache_set %s %s\n", key, value));
    int retval = mc_set(self->mc, key, keylen, value, valuelen, time, 0);
    debug(("retval = %d\n", retval));
    Py_INCREF(Py_None);
    return Py_None;
}

//----------------------------------------------------------------------------------------
//
static PyObject*
cmemcache_get(PyObject* pyself, PyObject* args)
{
    CmemcacheObject* self = (CmemcacheObject*)pyself;
    
    debug(("cmemcache_get\n"));

    assert(self->mc);
    
    char* key = NULL;
    int keylen = 0;

    if (! PyArg_ParseTuple(args, "s#", &key, &keylen))
        return NULL;
    debug(("cmemcache_get %s\n", key));
    
    struct memcache_req *req;
    struct memcache_res *res;
    req = mc_req_new();
    res = mc_req_add(req, key, keylen);
    mc_res_free_on_delete(res, 1);
    mc_get(self->mc, req);
    debug(("retval = %d %s\n", res->size, (char*)res->val));
    PyObject* retVal;
    retVal = PyString_FromStringAndSize(res->val, res->size);
    mc_req_free(req);
    return retVal;
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

    if (! PyArg_ParseTuple(args, "s#|d", &key, &keylen))
        return NULL;
    debug(("cmemcache_delete %s time %d\n", key, time));

    int retVal = 0;
    retVal = mc_delete(self->mc, key, keylen, time);
    debug(("retVal %d\n", retVal));
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef cmemcache_methods[] = {
    {
        "set", cmemcache_set, METH_VARARGS,
        "Client.set(key, value, time=0) -- set key to value"
    },
    {
        "get", cmemcache_get, METH_VARARGS,
        "Client.get(key) -- get value for key"
    },
    {
        "delete", cmemcache_delete, METH_VARARGS,
        "Client.delete(key, time=0) -- delete key"
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
    "cmemcache object",        /* tp_doc */
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
initcmemcache(void) 
{
    PyObject* m;
    
    cmemcache_CmemcacheType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&cmemcache_CmemcacheType) < 0)
        return;

    m = Py_InitModule3("cmemcache", cmemcache_module_methods,
                       "Fast client to memcached.");

    Py_INCREF(&cmemcache_CmemcacheType);
    PyModule_AddObject(m, "Client", (PyObject *)&cmemcache_CmemcacheType);
}
