/*
  flvscreen.c - providing FlvScreen class for Python.
  -*- tab-width: 4 -*-

  Copyright (c) 2009 by Yusuke Shinyama
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Python.h>
#include <structmember.h>

#ifdef _cplusplus
#define NULL 0
#else
#define NULL ((void *)0)
#endif

#ifndef PyVarObject_HEAD_INIT
#define PyVarObject_HEAD_INIT(type, size) PyObject_HEAD_INIT(type) size,
#endif

/*  FLVScreen
 */

/* RGBAPixel */
typedef struct _RGBAPixel
{
    unsigned char red;
    unsigned char green;
    unsigned char blue;
    unsigned char alpha;
} RGBAPixel;

/* FLVPixel */
typedef struct _FLVPixel
{
    unsigned char blue;
    unsigned char green;
    unsigned char red;
} FLVPixel;

/* FlvScreen class internal data */
typedef struct _PyFlvScreen
{
    PyObject_HEAD int blk_size;
    int blk_width, blk_height;
    int pix_width, pix_height;
    unsigned char *blocks;
    RGBAPixel *pixels;
    FLVPixel *tmppix;
} PyFlvScreen;


/* Utility functions
 */
static int idiv(int x, int y)
{
    if (0 <= x)
    {
        return x / y;
    }
    else
    {
        return (x - y + 1) / y;
    }
}

/* FlvScreen(block_size, width, height)
 *   Constructor.
 */
static int
FlvScreen_init(PyFlvScreen *self, PyObject *args, PyObject *kwargs)
{
    int blk_size, blk_width, blk_height;
    if (!PyArg_ParseTuple(args, "iii", &blk_size, &blk_width, &blk_height))
        return -1;
    if (blk_size <= 0 || blk_width <= 0 || blk_height <= 0)
        return -1;
    self->blk_size = blk_size;
    self->blk_width = blk_width;
    self->blk_height = blk_height;
    self->blocks = PyMem_Malloc(blk_width * blk_height);
    if (self->blocks == NULL)
        return -1;
    self->tmppix = PyMem_Malloc(blk_size * blk_size * sizeof(FLVPixel));
    if (self->tmppix == NULL)
        return -1;
    memset(self->blocks, 1, blk_width * blk_height);
    self->pix_width = blk_width * blk_size;
    self->pix_height = blk_height * blk_size;
    self->pixels = PyMem_Malloc(self->pix_width * self->pix_height * sizeof(RGBAPixel));
    if (self->pixels == NULL)
        return -1;
    memset(self->pixels, 0, self->pix_width * self->pix_height * sizeof(RGBAPixel));
    return 0;
}

/* FlvScreen allocator.
 */
static PyObject *
FlvScreen_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyFlvScreen *self;
    self = (PyFlvScreen *)type->tp_alloc(type, 0);
    if (self == NULL)
        return NULL;
    self->blocks = NULL;
    self->pixels = NULL;
    self->tmppix = NULL;
    return (PyObject *)self;
}

/* FlvScreen destructor.
 */
static void
FlvScreen_dealloc(PyFlvScreen *self)
{
    Py_TYPE(self)->tp_free((PyObject *)self);
    if (self->blocks != NULL)
    {
        PyMem_Free(self->blocks);
    }
    if (self->pixels != NULL)
    {
        PyMem_Free(self->pixels);
    }
    if (self->tmppix != NULL)
    {
        PyMem_Free(self->tmppix);
    }
}

/* FlvScreen.blit_rgba(x,y,w,h,data)
 *   copy the rgba data.
 */
static PyObject *
blit_rgba(PyFlvScreen *self, PyObject *args)
{
    PyObject *data;
    int px, py, pw, ph;
    int changes = 0;
    int blk_size = self->blk_size;

    if (!PyArg_ParseTuple(args, "iiiiO", &px, &py, &pw, &ph, &data))
    {
        return NULL;
    }

    /* check the data type */
    if (!PyBytes_CheckExact(data))
    {
        PyErr_SetString(PyExc_TypeError, "must be bytes");
        return NULL;
    }
    /* check the data size */
    if (PyBytes_Size(data) != pw * ph * sizeof(RGBAPixel))
    {
        PyErr_SetString(PyExc_RuntimeError, "invalid data size");
        return NULL;
    }

    /* copy the image data */
    {
        RGBAPixel *src = (RGBAPixel *)PyBytes_AsString(data);
        int bx0 = idiv(px, blk_size);
        int bx1 = idiv(px + pw - 1, blk_size);
        for (; 0 < ph; ph--, py++, src += pw)
        {
            int bx;
            int by = idiv(py, blk_size);
            int changed = 0;
            if (by < 0 || self->blk_height <= by)
                continue;
            for (bx = bx0; bx <= bx1; bx++)
            {
                int px0 = bx * blk_size;
                int px1 = (bx + 1) * blk_size;
                unsigned char *blk = &self->blocks[by * self->blk_width + bx];
                RGBAPixel *dst = &self->pixels[py * self->pix_width + px0];
                int i, j, n;
                if (bx < 0 || self->blk_width <= bx)
                    continue;
                if (px0 < px)
                {
                    i = 0;
                    j = px - px0;
                    if (px + pw < px1)
                    {
                        /*     px
			 *     |<-pw->|
			 *  +--|------|-+
			 *  |  |<====>| |
			 *  px0         px1
			 */
                        n = pw;
                    }
                    else
                    {
                        /*     px
			 *     |<-pw-|-->
			 *  +--|-----+
			 *  |  |<===>|
			 *  px0      px1
			 */
                        n = px1 - px;
                    }
                }
                else
                {
                    i = px0 - px;
                    j = 0;
                    if (px + pw < px1)
                    {
                        /*  px
			 *  |<--|-pw->|
			 *      +-----|-+
			 *      |<===>| |
			 *      px0     px1
			 */
                        n = px + pw - px0;
                    }
                    else
                    {
                        /*  px
			 *  |<--|-pw-|-->|
			 *      +----+
			 *      |<==>|
			 *      px0  px1
			 */
                        n = blk_size;
                    }
                }
                n *= sizeof(RGBAPixel);
                if (memcmp(&dst[j], &src[i], n))
                {
                    *blk = 1;
                    changed = 1;
                }
                memcpy(&dst[j], &src[i], n);
            }
            if (changed)
            {
                changes++;
            }
        }
    }

    return PyLong_FromLong(changes);
}

/* FlvScreen.changed()
 *   returns a list of the marked blocks.
 */
static PyObject *
changed(PyFlvScreen *self)
{
    PyObject *result;

    result = PyList_New(0);
    if (result == NULL)
    {
        return NULL;
    }

    /* scan all the blocks in the screen. */
    {
        long y;
        for (y = self->blk_height - 1; 0 <= y; y--)
        {
            unsigned char *blk = self->blocks + self->blk_width * y;
            long x;
            for (x = 0; x < self->blk_width; x++, blk++)
            {
                if (*blk != 0)
                {
                    PyObject *tup = PyTuple_New(2);
                    if (tup == NULL)
                        return NULL;
                    PyTuple_SetItem(tup, 0, PyLong_FromLong(x));
                    PyTuple_SetItem(tup, 1, PyLong_FromLong(y));
                    PyList_Append(result, tup);
                    Py_DECREF(tup);
                }
            }
        }
    }

    return result;
}

/* FlvScreen.get(i, j)
 *   returns the FLV-aware data of a block bitmap.
 */
static PyObject *
get(PyFlvScreen *self, PyObject *args)
{
    PyObject *result;
    int x, y;

    if (!PyArg_ParseTuple(args, "ii", &x, &y))
    {
        return NULL;
    }

    if (x < 0 || y < 0 || self->blk_width <= x || self->blk_height <= y)
    {
        PyErr_SetString(PyExc_RuntimeError, "out of range");
        return NULL;
    }

    {
        /* flip vertically */
        int px = x * self->blk_size;
        int py = y * self->blk_size;
        int dy;
        for (dy = 0; dy < self->blk_size; dy++)
        {
            RGBAPixel *src = &self->pixels[(py + dy) * self->pix_width + px];
            FLVPixel *dst = &self->tmppix[(self->blk_size - 1 - dy) * self->blk_size];
            int dx;
            for (dx = 0; dx < self->blk_size; dx++, src++, dst++)
            {
                dst->red = src->red;
                dst->green = src->green;
                dst->blue = src->blue;
            }
        }
        result = PyBytes_FromStringAndSize((char *)self->tmppix,
                                           self->blk_size * self->blk_size * sizeof(FLVPixel));
    }

    return result;
}

/* FlvScreen.reset()
 *   mark everything unchanged.
 */
static PyObject *
reset(PyFlvScreen *self)
{
    memset(self->blocks, 0, self->blk_width * self->blk_height);
    Py_RETURN_NONE;
}

/* FlvScreen.dump()
 *   for debugging purpose.
 */
static PyObject *
dump(PyFlvScreen *self)
{
    fprintf(stderr, "PyFlvScreen: self=%p, pixels=%p (%dx%d), blk_size=%d, blocks=%p (%dx%d), tmppix=%p\n",
            self,
            self->pixels, self->pix_width, self->pix_height,
            self->blk_size,
            self->blocks, self->blk_width, self->blk_height,
            self->tmppix);
    {
        int y;
        unsigned char *blk = self->blocks;
        for (y = 0; y < self->blk_height; y++)
        {
            int x;
            fprintf(stderr, "  block %d: ", y);
            for (x = 0; x < self->blk_width; x++)
            {
                fprintf(stderr, "%c", (*blk == 0) ? '0' : '1');
                blk++;
            }
            fprintf(stderr, "\n");
        }
    }
    {
        int by;
        RGBAPixel *pix = (RGBAPixel *)self->pixels;
        for (by = 0; by < self->blk_height; by++)
        {
            int dy;
            for (dy = 0; dy < self->blk_size; dy++)
            {
                int y = by * self->blk_size + dy;
                int bx;
                fprintf(stderr, "  pixel %d: ", y);
                for (bx = 0; bx < self->blk_width; bx++)
                {
                    int dx;
                    for (dx = 0; dx < self->blk_size; dx++)
                    {
                        //int x = bx * blk_size + dx;
                        fprintf(stderr, "%02x%02x%02x ", pix->red, pix->green, pix->blue);
                        pix++;
                    }
                }
                fprintf(stderr, "\n");
            }
        }
    }
    Py_RETURN_NONE;
}

static PyMemberDef FlvScreen_members[] = {
    {"block_size", T_INT, offsetof(PyFlvScreen, blk_size), READONLY,
     "the size of square blocks"},
    {"block_width", T_INT, offsetof(PyFlvScreen, blk_width), READONLY,
     "the number of horizontal blocks"},
    {"block_height", T_INT, offsetof(PyFlvScreen, blk_height), READONLY,
     "the number of vertical blocks"},
    {"pixel_width", T_INT, offsetof(PyFlvScreen, pix_width), READONLY,
     "the number of horizontal pixels"},
    {"pixel_height", T_INT, offsetof(PyFlvScreen, pix_height), READONLY,
     "the number of vertical pixels"},
    {NULL},
};

#define GETSTATE(m) ((struct module_state *)PyModule_GetState(m))

struct module_state
{
    PyObject *error;
};

static int FlvScreen_traverse(PyObject *m, visitproc visit, void *arg)
{
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int FlvScreen_clear(PyObject *m)
{
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

/* Module functions
 */
static PyObject *
flv2rgba(PyObject *self, PyObject *args)
{
    PyObject *data;
    PyObject *result;
    int width, height;
    if (!PyArg_ParseTuple(args, "iiO", &width, &height, &data))
    {

        PyErr_SetString(PyExc_RuntimeError, "bosta here");
        return NULL;
    }
    /* check the data type 
     if (!PyBytes_CheckExact(data)) {
     PyErr_SetString(PyExc_TypeError, "must be string");
     return NULL;
     }*/
    if (PyBytes_Size(data) != (width * height * sizeof(FLVPixel)))
    {
        PyErr_SetString(PyExc_RuntimeError, "invalid data size");
        return NULL;
    }
    FLVPixel *src = (FLVPixel *)(data);
    RGBAPixel *tmp = (RGBAPixel *)PyMem_Malloc(width * height * sizeof(RGBAPixel));
    int y;
    if (tmp == NULL)
        return NULL;
    for (y = 0; y < height; y++)
    {
        RGBAPixel *dst = tmp + width * (height - 1 - y);
        int x;
        for (x = 0; x < width; x++, src++, dst++)
        {
            dst->red = src->red;
            dst->green = src->green;
            dst->blue = src->blue;
            dst->alpha = 0;
        }
    }
    result = PyBytes_FromStringAndSize((char *)tmp, width * height * sizeof(RGBAPixel));
    PyMem_Free(tmp);

    return result;
}

static const PyMethodDef FlvScreen_methods[] = {
    {"blit_rgba", (PyCFunction)blit_rgba, METH_VARARGS,
     "blit_rgba"},
    {"changed", (PyCFunction)changed, METH_NOARGS,
     "changed"},
    {"get", (PyCFunction)get, METH_VARARGS,
     "get"},
    {"reset", (PyCFunction)reset, METH_NOARGS,
     "reset"},
    {"dump", (PyCFunction)dump, METH_NOARGS,
     "dump"},
    {"flv2rgba", (PyCFunction)flv2rgba, METH_VARARGS,
     "flv2rgba"},
    {NULL},
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "flvscreen",
    NULL,
    sizeof(struct module_state),
    FlvScreen_methods,
    NULL,
    FlvScreen_traverse,
    FlvScreen_clear,
    NULL};

static char FlvScreen_doc[] = "FlvScreen";

static PyTypeObject FlvScreen_type = {
    PyVarObject_HEAD_INIT(NULL, 0) "flvscreen.FlvScreen", /* tp_name */
    sizeof(PyFlvScreen),                                  /* tp_basicsize */
    0,                                                    /* tp_itemsize */
    (destructor)FlvScreen_dealloc,                        /* tp_dealloc */
    0,                                                    /* tp_print */
    0,                                                    /* tp_getattr */
    0,                                                    /* tp_setattr */
    0,                                                    /* tp_compare */
    0,                                                    /* tp_repr */
    0,                                                    /* tp_as_number */
    0,                                                    /* tp_as_sequence */
    0,                                                    /* tp_as_mapping */
    0,                                                    /* tp_hash */
    0,                                                    /* tp_call */
    0,                                                    /* tp_str */
    0,                                                    /* tp_getattro */
    0,                                                    /* tp_setattro */
    0,                                                    /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,                                   /* tp_flags */
    FlvScreen_doc,                                        /* tp_doc */
    0,                                                    /* tp_traverse */
    0,                                                    /* tp_clear */
    0,                                                    /* tp_richcompare */
    0,                                                    /* tp_weaklistoffset */
    0,                                                    /* tp_iter */
    0,                                                    /* tp_iternext */
    FlvScreen_methods,                                    /* tp_methods */
    FlvScreen_members,                                    /* tp_members */
    0,                                                    /* tp_getset */
    0,                                                    /* tp_base */
    0,                                                    /* tp_dict */
    0,                                                    /* tp_descr_get */
    0,                                                    /* tp_descr_set */
    0,                                                    /* tp_dictoffset */
    (initproc)FlvScreen_init,                             /* tp_init */
    0,                                                    /* tp_alloc */
    (newfunc)FlvScreen_new,                               /* tp_new */
};

/* Module initialization */
PyMODINIT_FUNC
PyInit_flvscreen(void)
{

    PyObject *module;
    PyObject *dict;
    FlvScreen_type.tp_new = PyType_GenericNew;

    if (PyType_Ready(&FlvScreen_type) < 0)
    {
        printf(stderr, "PyType_Ready failed.... \n");
        return;
    }

    module = PyModule_Create(&moduledef);
    if (module == NULL)
    {
        printf(stderr, "module creation failed\n");
        return NULL;
    }
    dict = PyModule_GetDict(module);
    if (dict == NULL)
        return NULL;
    

    Py_INCREF(&FlvScreen_type);
    PyModule_AddObject(module, "FlvScreen", (PyObject *)&FlvScreen_type);
    
    struct module_state *st = GETSTATE(module);
    st->error = PyErr_NewException("flvscreen.FlvError", PyExc_RuntimeError, NULL);
    if (st->error == NULL)
    {
        Py_DECREF(module);
        Py_DECREF(&FlvScreen_type);
        return NULL;
    }
    PyModule_AddObject(module, "FlvError", st->error);
    return module;
}
