### Python C Functions 
Functions placed in the PyMethodDef array cannot return void, instead they must
return a `PyObject*`, for a void function, return a `PyTuple_New(0)`
