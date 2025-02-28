#include "stdafx.h"
#include "pyspt.h"
#include "wechatocr.h"

#if USE_PYTHON+0
static CWeChatOCR* g_ocr_obj = 0;
static PyObject* py_init(PyObject* self, PyObject* args)
{
	const char* exe;
	const char* wcdir;
	if (!PyArg_ParseTuple(args, "ss", &exe, &wcdir))
		return NULL;
	if (!g_ocr_obj) {
		tstring wexe = util::to_tstr(exe);
		tstring wwcdir = util::to_tstr(wcdir);
		auto obj = new CWeChatOCR(wexe.c_str(), wwcdir.c_str());
		if (obj->wait_connection(5000)) {
			g_ocr_obj = obj;
			Py_RETURN_TRUE;
		} else {
			delete obj;
			Py_RETURN_FALSE;
		}
	}
	Py_RETURN_TRUE;
}

static PyObject* py_destroy(PyObject* self, PyObject* args)
{
	if (g_ocr_obj) {
		delete g_ocr_obj;
		g_ocr_obj = 0;
	}
	Py_RETURN_NONE;
}

static PyObject* py_ocr(PyObject* self, PyObject* args) {
	const char* imgpath=0;
	if (!PyArg_ParseTuple(args, "s", &imgpath))
		return NULL;
	if (!g_ocr_obj) {
		PyErr_SetString(PyExc_RuntimeError, "wechatocr not initialized");
		return NULL;
	}

	CWeChatOCR::result_t res{};
	if (!g_ocr_obj->doOCR(imgpath, &res)) {
		PyErr_SetString(PyExc_RuntimeError, "wechatocr doOCR failed");
		return NULL;
	}
	
	PyObject* dict = PyDict_New();

	PyObject* tmpo = PyUnicode_FromString(res.imgpath.c_str());
	PyDict_SetItemString(dict, "imgpath", tmpo);
	Py_DECREF(tmpo);

	tmpo = PyLong_FromLong(res.errcode);
	PyDict_SetItemString(dict, "errcode", tmpo);
	Py_DECREF(tmpo);

	tmpo = PyLong_FromLong(res.width);
	PyDict_SetItemString(dict, "width", tmpo);
	Py_DECREF(tmpo);
	tmpo = PyLong_FromLong(res.height);
	PyDict_SetItemString(dict, "height", tmpo);
	Py_DECREF(tmpo);

	tmpo = PyList_New(res.ocr_response.size());
	size_t idx = 0;
	for (auto& kv : res.ocr_response) {
		PyObject* tmpo2 = PyDict_New();
		PyObject* tmpo3 = PyUnicode_FromString(kv.text.c_str());
		PyDict_SetItemString(tmpo2, "text", tmpo3);
		Py_DECREF(tmpo3);
		tmpo3 = PyFloat_FromDouble(kv.left);
		PyDict_SetItemString(tmpo2, "left", tmpo3);
		Py_DECREF(tmpo3);
		tmpo3 = PyFloat_FromDouble(kv.top);
		PyDict_SetItemString(tmpo2, "top", tmpo3);
		Py_DECREF(tmpo3);
		tmpo3 = PyFloat_FromDouble(kv.right);
		PyDict_SetItemString(tmpo2, "right", tmpo3);
		Py_DECREF(tmpo3);
		tmpo3 = PyFloat_FromDouble(kv.bottom);
		PyDict_SetItemString(tmpo2, "bottom", tmpo3);
		Py_DECREF(tmpo3);
		tmpo3 = PyFloat_FromDouble(kv.rate);
		PyDict_SetItemString(tmpo2, "rate", tmpo3);
		Py_DECREF(tmpo3);
		PyList_SetItem(tmpo, idx++, tmpo2);
		// Py_DECREF(tmpo2); // note: no need to decref tmpo2, PyList_SetItem already steals the reference.
	}
	PyDict_SetItemString(dict, "ocr_response", tmpo);
	Py_DECREF(tmpo);
	return dict;
}

extern "C" EXPORTED_API
PyObject* PyInit_wcocr(void)
{
	static PyMethodDef wcocr_methods[] = {
		{ "init", py_init, METH_VARARGS, "init(wechatocr_exe_path, wechat_dir)=>bool\ninit wechatocr" },
		{ "destroy", py_destroy, METH_VARARGS, "destroy()=>deinit wechatocr" },
		{ "ocr", py_ocr, METH_VARARGS, "ocr(imgpath)=>dict" },
		{ NULL, NULL, 0, NULL }
	};
	static PyModuleDef wcocr_module = {
		PyModuleDef_HEAD_INIT,
		"wcocr",
		"wechatocr",
		0, // size
		wcocr_methods,
		NULL, // slots
	};
	PyObject* m = PyModule_Create(&wcocr_module);
	return m;
}
#endif
