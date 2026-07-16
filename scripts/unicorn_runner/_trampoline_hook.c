/*
 * C extension to replace Python trampoline hook callback in Unicorn engine.
 *
 * Eliminates ~5.3M C→Python→C round-trips during PPC emulation by keeping
 * the hook callback entirely in C, then building the Python tuple list
 * once after execution completes.
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <unicorn/unicorn.h>
#include <unicorn/ppc.h>
#include <stdint.h>

#define MAX_CALLS 50000

typedef struct {
    uint32_t index;
    uint32_t tramp_addr;
    uint32_t src_offset;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
} call_entry_t;

static call_entry_t g_log[MAX_CALLS];
static uint32_t g_log_count = 0;
static uint64_t g_code_base_plus4 = 0;
static uc_hook g_hook_handle = 0;

/* Registers to batch-read: LR, r3, r4, r5, r6 */
static const int g_regs[5] = {
    UC_PPC_REG_LR, UC_PPC_REG_3, UC_PPC_REG_4,
    UC_PPC_REG_5, UC_PPC_REG_6
};

static void trampoline_cb(uc_engine *uc, uint64_t address, uint32_t size,
                           void *user_data)
{
    if (address & 7)
        return;
    if (g_log_count >= MAX_CALLS)
        return;

    int vals[5];
    void *ptrs[5] = { &vals[0], &vals[1], &vals[2], &vals[3], &vals[4] };
    uc_reg_read_batch(uc, (int *)g_regs, ptrs, 5);

    call_entry_t *e = &g_log[g_log_count];
    e->index = g_log_count;
    e->tramp_addr = (uint32_t)address;
    e->src_offset = (uint32_t)(vals[0] - g_code_base_plus4);
    e->r3 = (uint32_t)vals[1];
    e->r4 = (uint32_t)vals[2];
    e->r5 = (uint32_t)vals[3];
    e->r6 = (uint32_t)vals[4];
    g_log_count++;
}

static PyObject *py_install_hook(PyObject *self, PyObject *args)
{
    unsigned long long uc_handle;
    unsigned long long tramp_base, tramp_end;
    unsigned long long code_base;

    if (!PyArg_ParseTuple(args, "KKKK", &uc_handle, &tramp_base,
                          &tramp_end, &code_base))
        return NULL;

    uc_engine *uc = (uc_engine *)(uintptr_t)uc_handle;
    g_code_base_plus4 = code_base + 4;

    uc_err err = uc_hook_add(uc, &g_hook_handle, UC_HOOK_BLOCK,
                             (void *)trampoline_cb, NULL,
                             tramp_base, tramp_end);
    if (err != UC_ERR_OK) {
        PyErr_Format(PyExc_RuntimeError, "uc_hook_add failed: %s",
                     uc_strerror(err));
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject *py_clear_log(PyObject *self, PyObject *args)
{
    g_log_count = 0;
    Py_RETURN_NONE;
}

static PyObject *py_get_log(PyObject *self, PyObject *args)
{
    PyObject *list = PyList_New(g_log_count);
    if (!list)
        return NULL;

    for (uint32_t i = 0; i < g_log_count; i++) {
        call_entry_t *e = &g_log[i];
        PyObject *tup = Py_BuildValue("(IIIIIII)",
            e->index, e->tramp_addr, e->src_offset,
            e->r3, e->r4, e->r5, e->r6);
        if (!tup) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, i, tup);
    }
    return list;
}

static PyMethodDef methods[] = {
    {"install_hook", py_install_hook, METH_VARARGS,
     "Install C trampoline hook on a Unicorn engine."},
    {"clear_log", py_clear_log, METH_NOARGS,
     "Reset the call log."},
    {"get_log", py_get_log, METH_NOARGS,
     "Get call log as list of 7-tuples."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "_trampoline_hook",
    "C trampoline hook for Unicorn PPC engine",
    -1,
    methods
};

PyMODINIT_FUNC PyInit__trampoline_hook(void)
{
    return PyModule_Create(&module);
}
