#include <Python.h>

#include <gcode/plan/Planner.h>

#include <cbang/json/NullSink.h>


class PyJSONSink : public cb::JSON::NullSink {
  PyObject *root;

  typedef std::vector<PyObject *> stack_t;
  stack_t stack;
  std::string key;

public:
  PyJSONSink() : root(0) {}

  PyObject *getRoot() {return root;}

  // From cb::JSON::Sink
  void writeNull() {
    cb::JSON::NullSink::writeNull();
    Py_INCREF(Py_None);
    add(Py_None);
  }


  void writeBoolean(bool value) {
    cb::JSON::NullSink::writeBoolean(value);
    PyObject *o = value ? Py_True : Py_False;
    Py_INCREF(o);
    add(o);
  }


  void write(double value) {
    cb::JSON::NullSink::write(value);
    add(PyFloat_FromDouble(value));
  }


  void write(int8_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromLong(value));
  }


  void write(uint8_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromUnsignedLong(value));
  }


  void write(int16_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromLong(value));
  }


  void write(uint16_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromUnsignedLong(value));
  }


  void write(int32_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromLong(value));
  }


  void write(uint32_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromUnsignedLong(value));
  }


  void write(int64_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromLongLong(value));
  }


  void write(uint64_t value) {
    cb::JSON::NullSink::write((double)value);
    add(PyLong_FromUnsignedLongLong(value));
  }


  void write(const std::string &value) {
    cb::JSON::NullSink::write(value);
    add(createString(value));
  }


  void beginList(bool simple = false) {
    push(PyList_New(0));
    cb::JSON::NullSink::beginList(false);
  }


  void beginAppend() {cb::JSON::NullSink::beginAppend();}


  void endList() {
    cb::JSON::NullSink::endList();
    pop();
  }


  void beginDict(bool simple = false) {
    push(PyDict_New());
    cb::JSON::NullSink::beginDict(simple);
  }


  void beginInsert(const std::string &key) {
    cb::JSON::NullSink::beginInsert(key);
    this->key = key;
  }


  void endDict() {
    cb::JSON::NullSink::endDict();
    pop();
  }


  PyObject *createString(const std::string &s) {
    return PyUnicode_FromStringAndSize(CPP_TO_C_STR(s), s.length());
  }


  void push(PyObject *o) {
    add(o);
    stack.push_back(o);
  }


  void pop() {stack.pop_back();}


  void add(PyObject *o) {
    if (!o) THROW("Cannot add null");

    if (!root) root = o;

    else if (inList()) {
      int ret = PyList_Append(stack.back(), o);

      Py_DECREF(o);

      if (ret) THROW("Append failed");

    } else if (inDict()) {
      PyObject *key = createString(this->key);

      int ret = PyDict_SetItem(stack.back(), key, o);

      Py_DECREF(key);
      Py_DECREF(o);

      if (ret) THROW("Insert failed");
    }
  }
};


typedef struct {
  PyObject_HEAD;
  GCode::Planner *planner;
} PyPlanner;


static void _dealloc(PyPlanner *self) {
  if (self->planner) delete self->planner;
  Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyObject *_new(PyTypeObject *type, PyObject *args, PyObject *kwds) {
  PyPlanner *self = (PyPlanner *)type->tp_alloc(type, 0);
  return (PyObject *)self;
}


static int _init(PyPlanner *self, PyObject *args, PyObject *kwds) {
  GCode::PlannerConfig config;

  PyObject *_config = 0;

  static const char *kwlist[] = {"config", 0};

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|O", (char **)kwlist,
                                   &_config))
    return -1;

  // TODO Convert Python object to JSON config

  self->planner = new GCode::Planner(config);

  return 0;
}


static PyObject *_is_running(PyPlanner *self) {
  if (self->planner->isRunning()) Py_RETURN_TRUE;
  else Py_RETURN_FALSE;
}


static PyObject *_mdi(PyPlanner *self, PyObject *args) {
  const char *gcode;

  if (!PyArg_ParseTuple(args, "s", &gcode)) return 0;

  self->planner->mdi(gcode);

  Py_RETURN_NONE;
}


static PyObject *_load(PyPlanner *self, PyObject *args) {
  const char *filename;

  if (!PyArg_ParseTuple(args, "s", &filename)) return 0;

  self->planner->load(std::string(filename));

  Py_RETURN_NONE;
}


static PyObject *_has_more(PyPlanner *self) {
  if (self->planner->hasMore()) Py_RETURN_TRUE;
  else Py_RETURN_FALSE;
}


static PyObject *_next(PyPlanner *self) {
  PyJSONSink sink;
  self->planner->next(sink);
  return sink.getRoot();
}


static PyObject *_release(PyPlanner *self, PyObject *args) {
  uint64_t line;

  if (!PyArg_ParseTuple(args, "K", &line)) return 0;

  self->planner->release(line);

  Py_RETURN_NONE;
}


static PyObject *_restart(PyPlanner *self, PyObject *args) {
  uint64_t line;
  double length;

  if (!PyArg_ParseTuple(args, "Kd", &line, &length)) return 0;

  self->planner->restart(line, length);

  Py_RETURN_NONE;
}


static PyMethodDef _methods[] = {
  {"is_running", (PyCFunction)_is_running, METH_NOARGS,
   "True if the planner active"},
  {"mdi", (PyCFunction)_mdi, METH_VARARGS, "Load MDI commands"},
  {"load", (PyCFunction)_load, METH_VARARGS, "Load GCode by filename"},
  {"has_more", (PyCFunction)_has_more, METH_NOARGS,
   "True if the planner has more data"},
  {"next", (PyCFunction)_next, METH_NOARGS, "Get next planner data"},
  {"release", (PyCFunction)_release, METH_VARARGS, "Release planner data"},
  {"restart", (PyCFunction)_restart, METH_VARARGS,
   "Restart planner from given line"},
  {0}
};


static PyTypeObject PlannerType = {
  PyVarObject_HEAD_INIT(0, 0)
  "gplan.Planner",           // tp_name
  sizeof(PyPlanner),         // tp_basicsize
  0,                         // tp_itemsize
  (destructor)_dealloc,      // tp_dealloc
  0,                         // tp_print
  0,                         // tp_getattr
  0,                         // tp_setattr
  0,                         // tp_reserved
  0,                         // tp_repr
  0,                         // tp_as_number
  0,                         // tp_as_sequence
  0,                         // tp_as_mapping
  0,                         // tp_hash
  0,                         // tp_call
  0,                         // tp_str
  0,                         // tp_getattro
  0,                         // tp_setattro
  0,                         // tp_as_buffer
  Py_TPFLAGS_DEFAULT |
  Py_TPFLAGS_BASETYPE,       // tp_flags
  "Planner object",          // tp_doc
  0,                         // tp_traverse
  0,                         // tp_clear
  0,                         // tp_richcompare
  0,                         // tp_weaklistoffset
  0,                         // tp_iter
  0,                         // tp_iternext
  _methods,                  // tp_methods
  0,                         // tp_members
  0,                         // tp_getset
  0,                         // tp_base
  0,                         // tp_dict
  0,                         // tp_descr_get
  0,                         // tp_descr_set
  0,                         // tp_dictoffset
  (initproc)_init,           // tp_init
  0,                         // tp_alloc
  _new,                      // tp_new
};


static struct PyModuleDef module = {
  PyModuleDef_HEAD_INIT, "gplan", 0, -1, 0, 0, 0, 0, 0
};


PyMODINIT_FUNC PyInit_gplan() {
  if (PyType_Ready(&PlannerType) < 0) return 0;

  PyObject *m = PyModule_Create(&module);
  if (!m) return 0;

  Py_INCREF(&PlannerType);
  PyModule_AddObject(m, "Planner", (PyObject *)&PlannerType);

  return m;
}
