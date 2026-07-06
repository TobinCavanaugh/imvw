//
// Created by tobin on 2025-02-23.
//

#include "dialect.h"

#ifndef PYTHON_LOADER_H
#define PYTHON_LOADER_H

#define ERR_PY(errname, ...) fprintf(stderr, "ERR|PY: " errname"\n\t@ %s:%d\n", __VA_ARGS__, __FILE__, __LINE__);

PyObject *test_func(PyObject *self, PyObject *args) {
    printf("Ran from python\n");
    return PyTuple_New(0);
}

static PyMethodDef ExposedFunctions[] = {
        {"testfunc", test_func, METH_VARARGS, ""},
        {NULL, NULL, 0, NULL}
};

static PyModuleDef main_module = {
        PyModuleDef_HEAD_INIT,
        "imvw",
        NULL,
        -1,
        ExposedFunctions
};

PyMODINIT_FUNC PyInit_main_module(void) {
    return PyModule_Create(&main_module);
}

PyObject **scripts_array = NULL;
i32 scripts_count = 0;

u0 scripts_add(char *name) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", ASSETS_PATH, name);

    if (!Py_IsInitialized()) {
        fprintf(stderr, "Attempting to run python scripts with python uninitialized. Run Enable_Python() please.\n");
        return;
    }

    // Test if file exists
    char check_path[512];
    snprintf(check_path, sizeof(check_path), "%s.py", full_path);
    if (!FileExists(check_path)) {
        ERR_PY("Failed to load script `%s` due to file not existing", check_path)
        return;
    }

    PyObject *pyname = PyUnicode_FromString(full_path);
    PyObject *mod = PyImport_Import(pyname);
    Py_DECREF(pyname);

    if (!mod) {
        PyErr_Print();
        ERR_PY("Failed to import script `%s` into Python runtime", full_path)
        return;
    }

    PyModule_AddFunctions(mod, ExposedFunctions);

    scripts_array = (PyObject **) realloc(scripts_array, (scripts_count + 1) * sizeof(PyObject *));
    scripts_array[scripts_count] = mod;
    ++scripts_count;
}

u0 python_run_script_func(char **python_scripts_array, i32 python_scripts_count, char *func_name) {
    if (!Py_IsInitialized()) {
        fprintf(stderr, "Attempting to run python scripts with python uninitialized. Run Enable_Python() please.\n");
        return;
    }

    i32 i = 0;
    for (; i < scripts_count; i++) {
        PyObject *obj = scripts_array[i];

        if (obj == NULL) { continue; }

        PyObject *func = PyObject_GetAttrString(obj, func_name);
        if (!func) {
            PyErr_Clear();  // Clear AttributeError when function doesn't exist
            continue;
        }
        PyObject *rags = PyTuple_New(0);
        if (rags) {
            PyObject *res = PyObject_CallObject(func, rags);
            Py_XDECREF(res);
            PyErr_Clear();  // Clear any unhandled Python exception
            Py_DECREF(rags);
        }
        Py_DECREF(func);
    }
}

u0 load_python(cJSON *json, char ***out_scripts, i32 *out_count) {
    cJSON *scripts = cJSON_GetObjectItem(json, "scripts");

    i32 size = cJSON_GetArraySize(scripts);
    *out_scripts = (char **) realloc(*out_scripts, sizeof(char *) * size);
    *out_count = size;

    i32 i = 0;
    for (; i < size; i++) {
        char *script_path = cJSON_GetStringValue(cJSON_GetArrayItem(scripts, i));
        char *res = (char *) (malloc(strlen(script_path) + 1));
        strcpy(res, script_path);

        // Remove the .py extension, because the python script runner doesn't
        // like it.
        char *ext = ".py";
        size_t slen = strlen(res);
        size_t tlen = strlen(ext);
        if (!strcmp(res + slen - tlen, ext)) {
            res[strlen(res) - strlen(ext)] = '\0';
        }

        (*out_scripts)[i] = res;

        scripts_add(res);
    }
}

#endif //PYTHON_LOADER_H
