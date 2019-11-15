/*
 * Copyright 2014-2019, CNRS
 * Copyright 2018-2019, INRIA
 */

#ifndef __eigenpy_details_hpp__
#define __eigenpy_details_hpp__

#include "eigenpy/details/rvalue_from_python_data.hpp"
#include "eigenpy/fwd.hpp"

#include <patchlevel.h> // For PY_MAJOR_VERSION
#include <numpy/arrayobject.h>
#include <iostream>

#include "eigenpy/eigenpy.hpp"
#include "eigenpy/registration.hpp"
#include "eigenpy/map.hpp"

#define GET_PY_ARRAY_TYPE(array) PyArray_ObjectType(reinterpret_cast<PyObject *>(array), 0)

namespace eigenpy
{
  template <typename SCALAR>  struct NumpyEquivalentType {};
  template <> struct NumpyEquivalentType<double>  { enum { type_code = NPY_DOUBLE };};
  template <> struct NumpyEquivalentType<int>     { enum { type_code = NPY_INT    };};
  template <> struct NumpyEquivalentType<long>    { enum { type_code = NPY_LONG    };};
  template <> struct NumpyEquivalentType<float>   { enum { type_code = NPY_FLOAT  };};
  
  template <typename SCALAR1, typename SCALAR2>
  struct FromTypeToType : public boost::false_type {};
  
  template <typename SCALAR>
  struct FromTypeToType<SCALAR,SCALAR> : public boost::true_type {};
  
  template <> struct FromTypeToType<int,long> : public boost::true_type {};
  template <> struct FromTypeToType<int,float> : public boost::true_type {};
  template <> struct FromTypeToType<int,double> : public boost::true_type {};
  
  template <> struct FromTypeToType<long,float> : public boost::true_type {};
  template <> struct FromTypeToType<long,double> : public boost::true_type {};
  
  template <> struct FromTypeToType<float,double> : public boost::true_type {};

  namespace bp = boost::python;

  enum NP_TYPE
  {
    DEFAULT_TYPE,
    MATRIX_TYPE,
    ARRAY_TYPE
  };
  
  struct NumpyType
  {
    
    static NumpyType & getInstance()
    {
      static NumpyType instance;
      return instance;
    }

    operator bp::object () { return CurrentNumpyType; }

    bp::object make(PyArrayObject* pyArray, bool copy = false)
    { return make((PyObject*)pyArray,copy); }
    
    bp::object make(PyObject* pyObj, bool copy = false)
    {
      if (getType() == DEFAULT_TYPE) {
        std::cerr <<
          "eigenpy warning: you use the deprecated class numpy.matrix without explicily asking for it. "
          "The default behaviour will change to numpy.array at next major release.\n"
          "- Either call eigenpy.switchToNumpyMatrix() before using eigenpy to suppress this warning\n"
          "- or call eigenpy.switchToNumpyArray() and adapt your code accordingly.\n"
          "See https://github.com/stack-of-tasks/eigenpy/issues/87 for further details."
          << std::endl;
        switchToNumpyMatrix();
      }
      bp::object m;
      if(PyType_IsSubtype(reinterpret_cast<PyTypeObject*>(CurrentNumpyType.ptr()),NumpyMatrixType))
        m = NumpyMatrixObject(bp::object(bp::handle<>(pyObj)), bp::object(), copy);
//        m = NumpyAsMatrixObject(bp::object(bp::handle<>(pyObj)));
      else if(PyType_IsSubtype(reinterpret_cast<PyTypeObject*>(CurrentNumpyType.ptr()),NumpyArrayType))
        m = bp::object(bp::handle<>(pyObj)); // nothing to do here

      Py_INCREF(m.ptr());
      return m;
    }
    
    static void setNumpyType(bp::object & obj)
    {
      PyTypeObject * obj_type = PyType_Check(obj.ptr()) ? reinterpret_cast<PyTypeObject*>(obj.ptr()) : obj.ptr()->ob_type;
      if(PyType_IsSubtype(obj_type,getInstance().NumpyMatrixType))
        switchToNumpyMatrix();
      else if(PyType_IsSubtype(obj_type,getInstance().NumpyArrayType))
        switchToNumpyArray();
    }
    
    static void switchToNumpyArray()
    {
      getInstance().CurrentNumpyType = getInstance().NumpyArrayObject;
      getType() = ARRAY_TYPE;
    }
    
    static void switchToNumpyMatrix()
    {
      getInstance().CurrentNumpyType = getInstance().NumpyMatrixObject;
      getType() = MATRIX_TYPE;
    }
    
    static NP_TYPE & getType()
    {
      static NP_TYPE np_type;
      return np_type;
    }
    
    static bp::object getNumpyType()
    {
      return getInstance().CurrentNumpyType;
    }
    
    static const PyTypeObject * getNumpyMatrixType()
    {
      return getInstance().NumpyMatrixType;
    }
    
    static const PyTypeObject * getNumpyArrayType()
    {
      return getInstance().NumpyArrayType;
    }
    
    static bool isMatrix()
    {
      return PyType_IsSubtype(reinterpret_cast<PyTypeObject*>(getInstance().CurrentNumpyType.ptr()),
                              getInstance().NumpyMatrixType);
    }
    
    static bool isArray()
    {
      return PyType_IsSubtype(reinterpret_cast<PyTypeObject*>(getInstance().CurrentNumpyType.ptr()),
                              getInstance().NumpyArrayType);
    }

  protected:
    NumpyType()
    {
      pyModule = bp::import("numpy");
#if PY_MAJOR_VERSION >= 3
      // TODO I don't know why this Py_INCREF is necessary.
      // Without it, the destructor of NumpyType SEGV sometimes.
      Py_INCREF(pyModule.ptr());
#endif
      
      NumpyMatrixObject = pyModule.attr("matrix");
      NumpyMatrixType = reinterpret_cast<PyTypeObject*>(NumpyMatrixObject.ptr());
      NumpyArrayObject = pyModule.attr("ndarray");
      NumpyArrayType = reinterpret_cast<PyTypeObject*>(NumpyArrayObject.ptr());
      //NumpyAsMatrixObject = pyModule.attr("asmatrix");
      //NumpyAsMatrixType = reinterpret_cast<PyTypeObject*>(NumpyAsMatrixObject.ptr());
      
      CurrentNumpyType = NumpyMatrixObject; // default conversion
      getType() = DEFAULT_TYPE;
    }

    bp::object CurrentNumpyType;
    bp::object pyModule;
    
    // Numpy types
    bp::object NumpyMatrixObject; PyTypeObject * NumpyMatrixType;
    //bp::object NumpyAsMatrixObject; PyTypeObject * NumpyAsMatrixType;
    bp::object NumpyArrayObject; PyTypeObject * NumpyArrayType;
    
  };

  template<typename MatType, bool IsVectorAtCompileTime = MatType::IsVectorAtCompileTime>
  struct initEigenObject
  {
    static MatType * run(PyArrayObject * pyArray, void * storage)
    {
      assert(PyArray_NDIM(pyArray) == 2);

      const int rows = (int)PyArray_DIMS(pyArray)[0];
      const int cols = (int)PyArray_DIMS(pyArray)[1];
      
      return new (storage) MatType(rows,cols);
    }
  };

  template<typename MatType>
  struct initEigenObject<MatType,true>
  {
    static MatType * run(PyArrayObject * pyArray, void * storage)
    {
      if(PyArray_NDIM(pyArray) == 1)
      {
        const int rows_or_cols = (int)PyArray_DIMS(pyArray)[0];
        return new (storage) MatType(rows_or_cols);
      }
      else
      {
        const int rows = (int)PyArray_DIMS(pyArray)[0];
        const int cols = (int)PyArray_DIMS(pyArray)[1];
        return new (storage) MatType(rows,cols);
      }
    }
  };
  
  template<typename MatType>
  struct EigenObjectAllocator
  {
    typedef MatType Type;
    typedef typename MatType::Scalar Scalar;
    
    static void allocate(PyArrayObject * pyArray, void * storage)
    {
      Type * mat_ptr = initEigenObject<Type>::run(pyArray,storage);
      
      if(NumpyEquivalentType<Scalar>::type_code == GET_PY_ARRAY_TYPE(pyArray))
      {
        *mat_ptr = MapNumpy<MatType,Scalar>::map(pyArray); // avoid useless cast
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_INT)
      {
        *mat_ptr = MapNumpy<MatType,int>::map(pyArray).template cast<Scalar>();
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_LONG)
      {
        *mat_ptr = MapNumpy<MatType,long>::map(pyArray).template cast<Scalar>();
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_FLOAT)
      {
        *mat_ptr = MapNumpy<MatType,float>::map(pyArray).template cast<Scalar>();
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_DOUBLE)
      {
        *mat_ptr = MapNumpy<MatType,double>::map(pyArray).template cast<Scalar>();
        return;
      }
    }
    
    /// \brief Copy mat into the Python array using Eigen::Map
    template<typename MatrixDerived>
    static void copy(const Eigen::MatrixBase<MatrixDerived> & mat_,
                     PyArrayObject * pyArray)
    {
      const MatrixDerived & mat = const_cast<const MatrixDerived &>(mat_.derived());
      
      if(NumpyEquivalentType<Scalar>::type_code == GET_PY_ARRAY_TYPE(pyArray))
      {
        MapNumpy<MatType,Scalar>::map(pyArray) = mat; // no cast needed
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_INT)
      {
        MapNumpy<MatType,int>::map(pyArray) = mat.template cast<int>();
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_LONG)
      {
        MapNumpy<MatType,long>::map(pyArray) = mat.template cast<long>();
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_FLOAT)
      {
        MapNumpy<MatType,float>::map(pyArray) = mat.template cast<float>();
        return;
      }
      
      if(GET_PY_ARRAY_TYPE(pyArray) == NPY_DOUBLE)
      {
        MapNumpy<MatType,double>::map(pyArray) = mat.template cast<double>();
        return;
      }
    }
  };
  
#if EIGEN_VERSION_AT_LEAST(3,2,0)
  template<typename MatType>
  struct EigenObjectAllocator< eigenpy::Ref<MatType> >
  {
    typedef eigenpy::Ref<MatType> Type;
    typedef typename MatType::Scalar Scalar;
    
    static void allocate(PyArrayObject * pyArray, void * storage)
    {
      typename MapNumpy<MatType,Scalar>::EigenMap numpyMap = MapNumpy<MatType,Scalar>::map(pyArray);
      new (storage) Type(numpyMap);
    }
    
    static void copy(Type const & mat, PyArrayObject * pyArray)
    {
      EigenObjectAllocator<MatType>::copy(mat,pyArray);
    }
  };
#endif
  
  /* --- TO PYTHON -------------------------------------------------------------- */
  
  template<typename MatType>
  struct EigenToPy
  {
    static PyObject* convert(MatType const & mat)
    {
      typedef typename MatType::Scalar Scalar;
      assert( (mat.rows()<INT_MAX) && (mat.cols()<INT_MAX) 
	      && "Matrix range larger than int ... should never happen." );
      const int R  = (int)mat.rows(), C = (int)mat.cols();

      PyArrayObject* pyArray;
      // Allocate Python memory
      if(C == 1 && NumpyType::getType() == ARRAY_TYPE) // Handle array with a single dimension
      {
        npy_intp shape[1] = { R };
        pyArray = (PyArrayObject*) PyArray_SimpleNew(1, shape,
                                                     NumpyEquivalentType<Scalar>::type_code);
      }
      else
      {
        npy_intp shape[2] = { R,C };
        pyArray = (PyArrayObject*) PyArray_SimpleNew(2, shape,
                                                     NumpyEquivalentType<Scalar>::type_code);
      }

      // Allocate memory
      EigenObjectAllocator<MatType>::copy(mat,pyArray);
      
      // Create an instance (either np.array or np.matrix)
      return NumpyType::getInstance().make(pyArray).ptr();
    }
  };
  
  /* --- FROM PYTHON ------------------------------------------------------------ */

  template<typename MatType>
  struct EigenFromPy
  {
    
    static bool isScalarConvertible(const int np_type)
    {
      if(NumpyEquivalentType<typename MatType::Scalar>::type_code == np_type)
        return true;
      
      switch(np_type)
      {
        case NPY_INT:
          return FromTypeToType<int,typename MatType::Scalar>::value;
        case NPY_LONG:
          return FromTypeToType<long,typename MatType::Scalar>::value;
        case NPY_FLOAT:
          return FromTypeToType<float,typename MatType::Scalar>::value;
        case NPY_DOUBLE:
          return FromTypeToType<double,typename MatType::Scalar>::value;
        default:
          return false;
      }
    }
    
    /// \brief Determine if pyObj can be converted into a MatType object
    static void* convertible(PyArrayObject* pyArray)
    {
      if(!PyArray_Check(pyArray))
        return 0;
      
      if(!isScalarConvertible(GET_PY_ARRAY_TYPE(pyArray)))
        return 0;

      if(MatType::IsVectorAtCompileTime)
      {
        switch(PyArray_NDIM(pyArray))
        {
          case 0:
            return 0;
          case 1:
            return pyArray;
          case 2:
          {
            // Special care of scalar matrix of dimension 1x1.
            if(PyArray_DIMS(pyArray)[0] == 1 && PyArray_DIMS(pyArray)[1] == 1)
              return pyArray;
            
            if(PyArray_DIMS(pyArray)[0] > 1 && PyArray_DIMS(pyArray)[1] > 1)
            {
#ifndef NDEBUG
              std::cerr << "The number of dimension of the object does not correspond to a vector" << std::endl;
#endif
              return 0;
            }
            
            if(((PyArray_DIMS(pyArray)[0] == 1) && (MatType::ColsAtCompileTime == 1))
               || ((PyArray_DIMS(pyArray)[1] == 1) && (MatType::RowsAtCompileTime == 1)))
            {
#ifndef NDEBUG
              if(MatType::ColsAtCompileTime == 1)
                std::cerr << "The object is not a column vector" << std::endl;
              else
                std::cerr << "The object is not a row vector" << std::endl;
#endif
              return 0;
            }
            break;
          }
          default:
            return 0;
        }
      }
      else // this is a matrix
      {
        if(PyArray_NDIM(pyArray) != 2)
        {
          if ( (PyArray_NDIM(pyArray) !=1) || (! MatType::IsVectorAtCompileTime) )
          {
#ifndef NDEBUG
            std::cerr << "The number of dimension of the object is not correct." << std::endl;
#endif
            return 0;
          }
        }
        
        if(PyArray_NDIM(pyArray) == 2)
        {
          const int R = (int)PyArray_DIMS(pyArray)[0];
          const int C = (int)PyArray_DIMS(pyArray)[1];
          
          if( (MatType::RowsAtCompileTime!=R)
             && (MatType::RowsAtCompileTime!=Eigen::Dynamic) )
            return 0;
          if( (MatType::ColsAtCompileTime!=C)
             && (MatType::ColsAtCompileTime!=Eigen::Dynamic) )
            return 0;
        }
      }
        
#ifdef NPY_1_8_API_VERSION
      if(!(PyArray_FLAGS(pyArray)))
#else
      if(!(PyArray_FLAGS(pyArray) & NPY_ALIGNED))
#endif
      {
#ifndef NDEBUG
        std::cerr << "NPY non-aligned matrices are not implemented." << std::endl;
#endif
        return 0;
      }
      
      return pyArray;
    }
 
    /// \brief Allocate memory and copy pyObj in the new storage
    static void construct(PyObject* pyObj,
                          bp::converter::rvalue_from_python_stage1_data* memory)
    {
      PyArrayObject * pyArray = reinterpret_cast<PyArrayObject*>(pyObj);
      assert((PyArray_DIMS(pyArray)[0]<INT_MAX) && (PyArray_DIMS(pyArray)[1]<INT_MAX));
      
      void* storage = ((bp::converter::rvalue_from_python_storage<MatType>*)
                       ((void*)memory))->storage.bytes;
      
      EigenObjectAllocator<MatType>::allocate(pyArray,storage);

      memory->convertible = storage;
    }
    
    static void registration()
    {
      bp::converter::registry::push_back
      (reinterpret_cast<void *(*)(_object *)>(&EigenFromPy::convertible),
       &EigenFromPy::construct,bp::type_id<MatType>());
    }
  };
  
  template<typename MatType>
  struct EigenFromPy< Eigen::MatrixBase<MatType> >
  {
    typedef EigenFromPy<MatType> EigenFromPyDerived;
    typedef Eigen::MatrixBase<MatType> Base;
    
    /// \brief Determine if pyObj can be converted into a MatType object
    static void* convertible(PyArrayObject* pyObj)
    {
      return EigenFromPyDerived::convertible(pyObj);
    }
    
    /// \brief Allocate memory and copy pyObj in the new storage
    static void construct(PyObject* pyObj,
                          bp::converter::rvalue_from_python_stage1_data* memory)
    {
      EigenFromPyDerived::construct(pyObj,memory);
    }
    
    static void registration()
    {
      bp::converter::registry::push_back
      (reinterpret_cast<void *(*)(_object *)>(&EigenFromPy::convertible),
       &EigenFromPy::construct,bp::type_id<Base>());
    }
  };
  
#define numpy_import_array() {if (_import_array() < 0) {PyErr_Print(); PyErr_SetString(PyExc_ImportError, "numpy.core.multiarray failed to import"); } }
  
  template<typename MatType,typename EigenEquivalentType>
  void enableEigenPySpecific()
  {
    enableEigenPySpecific<MatType>();
  }
  
  template<typename MatType>
  struct EigenFromPyConverter
  {
    static void registration()
    {
      EigenFromPy<MatType>::registration();

      // Add also conversion to Eigen::MatrixBase<MatType>
      typedef Eigen::MatrixBase<MatType> MatTypeBase;
      EigenFromPy<MatTypeBase>::registration();
    }
  };

#if EIGEN_VERSION_AT_LEAST(3,2,0)
  /// Template specialization for Eigen::Ref
  template<typename MatType>
  struct EigenFromPyConverter< eigenpy::Ref<MatType> >
  {
    static void registration()
    {
      bp::converter::registry::push_back
      (reinterpret_cast<void *(*)(_object *)>(&EigenFromPy<MatType>::convertible),
       &EigenFromPy<MatType>::construct,bp::type_id<MatType>());
    }
  };
#endif
  
  template<typename MatType>
  void enableEigenPySpecific()
  {
    numpy_import_array();
    if(check_registration<MatType>()) return;
    
    bp::to_python_converter<MatType,EigenToPy<MatType> >();
    EigenFromPyConverter<MatType>::registration();
   
  }

} // namespace eigenpy

#endif // ifndef __eigenpy_details_hpp__
