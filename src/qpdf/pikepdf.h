/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (C) 2017, James R. Barlow (https://github.com/jbarlow83/)
 */

#pragma once

#include <exception>
#include <qpdf/PointerHolder.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <pybind11/pybind11.h>

using uint = unsigned int;

#if __cplusplus < 201402L  // If C++11

#include <memory>
#include <type_traits>
#include <utility>
#include <string>

// Provide make_unique for C++11 (not array-capable)
// See https://stackoverflow.com/questions/17902405/how-to-implement-make-unique-function-in-c11/17902439#17902439 for full version if needed
namespace std {
    template<typename T, typename ...Args>
    unique_ptr<T> make_unique( Args&& ...args )
    {
        return unique_ptr<T>( new T( std::forward<Args>(args)... ) );
    }

    string quoted(const char* s);
    string quoted(const string &s);
};
#endif // }}

namespace pybind11 {
    PYBIND11_RUNTIME_EXCEPTION(attr_error, PyExc_AttributeError);
    PYBIND11_RUNTIME_EXCEPTION(notimpl_error, PyExc_NotImplementedError);
    PYBIND11_RUNTIME_EXCEPTION(filenotfound_error, PyExc_FileNotFoundError);
};

// Declare PointerHolder<T> as a smart pointer
// https://pybind11.readthedocs.io/en/stable/advanced/smart_ptrs.html#custom-smart-pointers
PYBIND11_DECLARE_HOLDER_TYPE(T, PointerHolder<T>);
namespace pybind11 { namespace detail {
    template <typename T>
    struct holder_helper<PointerHolder<T>> {
        static const T *get(const PointerHolder<T> &p) { return p.getPointer(); }
    };
}}

#define CUSTOM_TYPE_CONVERSION 1
#if CUSTOM_TYPE_CONVERSION

// From object_convert.cpp
pybind11::object decimal_from_pdfobject(QPDFObjectHandle& h);

namespace pybind11 { namespace detail {
    template <> struct type_caster<QPDFObjectHandle> : public type_caster_base<QPDFObjectHandle> {
        using base = type_caster_base<QPDFObjectHandle>;
    protected:
        QPDFObjectHandle value;
    public:

        /**
         * Conversion part 1 (Python->C++): convert a PyObject into a Object
         */
        bool load(handle src, bool convert) {
            // if (src.is_none()) {
            //     if (!convert) return false;
            //     value = QPDFObjectHandle::newNull();
            //     return true;
            // }
            // Attempting to construct these does not work...
            // if (convert) {
            //     if (PYBIND11_LONG_CHECK(src.ptr())) {
            //         auto as_int = src.cast<long long>();
            //         value = QPDFObjectHandle::newInteger(as_int);
            //     } /*else if (PyFloat_Check(src.ptr())) {
            //         auto as_double = src.cast<double>();
            //         value = QPDFObjectHandle::newReal(as_double);
            //     } */ else {
            //         return base::load(src, convert);
            //     }
            //     return true;
            // }
            return base::load(src, convert);
        }

        /**
         * Conversion part 2 (C++ -> Python): convert an instance into
         * a Python object.
         * Purpose of this is to establish the indirect keep_alive relationship
         * between QPDF and objects that refer back to in ways that pybind11
         * can't trace on its own.
         * We also convert several QPDFObjectHandle types to native Python
         * objects here.
         * The ==take_ownership code paths are currently unused but present
         * for completeness. They are unused because pybind11 only sets
         * take_ownership when a binding returns raw pointers to Python, and
         * by making this caster private we prohibit that.
         */
    private:
        // 'private': disallow returning pointers to QPDFObjectHandle from bindings
        static handle cast(const QPDFObjectHandle *csrc, return_value_policy policy, handle parent) {
            QPDFObjectHandle *src = const_cast<QPDFObjectHandle *>(csrc);
            if (!csrc)
                return none().release();

            bool primitive = true;
            handle h;

            switch (src->getTypeCode()) {
                case QPDFObject::object_type_e::ot_null:
                    h = none().release();
                    break;
                case QPDFObject::object_type_e::ot_integer:
                    h = int_(src->getIntValue()).release();
                    break;
                case QPDFObject::object_type_e::ot_boolean:
                    h = bool_(src->getBoolValue()).release();
                    break;
                case QPDFObject::object_type_e::ot_real:
                    h = decimal_from_pdfobject(*src).release();
                    break;
                default:
                    primitive = false;
                    break;
            }
            if (primitive && h) {
                if (policy == return_value_policy::take_ownership)
                    delete csrc;
                return h;
            }

            QPDF *owner = src->getOwningQPDF();
            if (policy == return_value_policy::take_ownership) {
                h = base::cast(std::move(*csrc), policy, parent);
                delete csrc;
            } else {
                h = base::cast(*csrc, policy, parent);
            }
            if (owner) {
                // Find the Python object that refers to our owner
                // Can do that by casting or more direct lookup
                //auto pyqpdf = pybind11::cast(owner);
                auto tinfo = get_type_info(typeid(QPDF));
                handle pyqpdf = get_object_handle(owner, tinfo);

                // Tell pybind11 that it must keep pyqpdf alive as long as h is
                // alive
                keep_alive_impl(h, pyqpdf);
            }
            return h;
        }

    public:
        static handle cast(QPDFObjectHandle &&src, return_value_policy policy, handle parent) {
            return cast(&src, return_value_policy::move, parent);
        }

        static handle cast(const QPDFObjectHandle &src, return_value_policy policy, handle parent) {
            if (policy == return_value_policy::automatic || policy == return_value_policy::automatic_reference)
                policy = return_value_policy::copy;
            return cast(&src, policy, parent);
        }
    };
}} // namespace pybind11::detail
#endif

namespace py = pybind11;

// From object.cpp
size_t list_range_check(QPDFObjectHandle& h, int index);
void init_object(py::module& m);

// From object_repr.cpp
std::string objecthandle_scalar_value(QPDFObjectHandle& h, bool escaped=true);
std::string objecthandle_pythonic_typename(QPDFObjectHandle& h, std::string prefix = "pikepdf.");
std::string objecthandle_repr_typename_and_value(QPDFObjectHandle& h);
std::string objecthandle_repr(QPDFObjectHandle& h);

// From object_convert.cpp
py::object decimal_from_pdfobject(QPDFObjectHandle& h);
py::object objecthandle_decode(QPDFObjectHandle& h);
QPDFObjectHandle objecthandle_encode(py::handle handle);
std::vector<QPDFObjectHandle> array_builder(py::iterable iter);
std::map<std::string, QPDFObjectHandle> dict_builder(py::dict dict);

// Support for recursion checks
class StackGuard
{
public:
    StackGuard(const char *where) {
        Py_EnterRecursiveCall(where);
    }
    StackGuard(const StackGuard&) = delete;
    StackGuard& operator= (const StackGuard&) = delete;
    ~StackGuard() {
        Py_LeaveRecursiveCall();
    }
};
