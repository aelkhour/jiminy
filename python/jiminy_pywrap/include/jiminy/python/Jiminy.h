///////////////////////////////////////////////////////////////////////////////
/// \brief             Python exposition functions for Jiminy project.
////////////////////////////////////////////////////////////////////////////////

#ifndef SIMULATOR_PYTHON_H
#define SIMULATOR_PYTHON_H

#include <cassert>

#include <boost/weak_ptr.hpp>
#include <boost/preprocessor.hpp>

#include <boost/python.hpp>
#include <boost/python/def.hpp>
#include <boost/python/dict.hpp>

#include "jiminy/core/Types.h"
#include "jiminy/core/Engine.h"
#include "jiminy/core/Model.h"
#include "jiminy/core/AbstractSensor.h"
#include "jiminy/core/Sensor.h"
#include "jiminy/core/AbstractController.h"
#include "jiminy/core/ControllerFunctor.h"
#include "jiminy/python/Utilities.h"


namespace jiminy
{
namespace python
{
    namespace bp = boost::python;

    typedef std::function<bool(float64_t const & /*t*/,
                               vectorN_t const & /*x*/)> callbackFct_t;

    // ************************** TimeStateFctPyWrapper ******************************

    template<typename T>
    PyObject * getNumpyReference(T & data)
    {
        return getNumpyReferenceFromScalar(data);
    }

    template<>
    PyObject * getNumpyReference<vector3_t>(vector3_t & data)
    {
        return getNumpyReferenceFromEigenVector(data);
    }

    template<typename T>
    struct TimeStateFctPyWrapper {
    public:
        // Disable the copy of the class
        TimeStateFctPyWrapper & operator = (TimeStateFctPyWrapper const & other) = delete;

    public:
        TimeStateFctPyWrapper(bp::object const& objPy) :
        funcPyPtr_(objPy),
        outPtr_(new T),
        outPyPtr_()
        {
            outPyPtr_ = getNumpyReference(*outPtr_);
        }

        // Copy constructor, same as the normal constructor
        TimeStateFctPyWrapper(TimeStateFctPyWrapper const & other) :
        funcPyPtr_(other.funcPyPtr_),
        outPtr_(new T),
        outPyPtr_()
        {
            *outPtr_ = *(other.outPtr_);
            outPyPtr_ = getNumpyReference(*outPtr_);
        }

        // Move constructor, takes a rvalue reference &&
        TimeStateFctPyWrapper(TimeStateFctPyWrapper&& other) :
        funcPyPtr_(other.funcPyPtr_),
        outPtr_(nullptr),
        outPyPtr_(nullptr)
        {
            // Steal the resource from "other"
            outPtr_ = other.outPtr_;
            outPyPtr_ = other.outPyPtr_;

            /* "other" will soon be destroyed and its destructor will
               do nothing because we null out its resource here */
            other.outPtr_ = nullptr;
            other.outPyPtr_ = nullptr;
        }

        // Destructor
        ~TimeStateFctPyWrapper()
        {
            delete outPtr_;
        }

        // Move assignment, takes a rvalue reference &&
        TimeStateFctPyWrapper& operator=(TimeStateFctPyWrapper&& other)
        {
            /* "other" is soon going to be destroyed, so we let it destroy our current resource
               instead and we take "other"'s current resource via swapping */
            std::swap(funcPyPtr_, other.funcPyPtr_);
            std::swap(outPtr_, other.outPtr_);
            std::swap(outPyPtr_, other.outPyPtr_);
            return *this;
        }

        T operator() (float64_t const & t,
                      vectorN_t const & x)
        {
            // Pass the arguments by reference (be careful const qualifiers are lost)
            bp::handle<> xPy(getNumpyReferenceFromEigenVector(x));
            bp::handle<> outPy(bp::borrowed(outPyPtr_));
            funcPyPtr_(t, xPy, outPy);
            return *outPtr_;
        }

    private:
        bp::object funcPyPtr_;
        T * outPtr_;
        PyObject * outPyPtr_; // Its lifetime in managed by boost::python
    };

    // ******************************  ***************************************

    template<std::size_t N, class = std::make_index_sequence<N> >
    struct ControllerFctWrapperN;

    template <size_t N, size_t... Is>
    struct ControllerFctWrapperN<N, std::index_sequence<Is...> >
    {
    private:
        template <size_t >
        using T_ = matrixN_t const &;

    public:
        ControllerFctWrapperN(bp::object const & objPy) : funcPyPtr_(objPy) {}
        void operator() (float64_t const & t,
                         vectorN_t const & q,
                         vectorN_t const & v,
                         T_<Is>... sensorsData,
                         vectorN_t       & uCommand)
        {
            // Pass the arguments by reference (be careful const qualifiers are lost).
            // Note that unlike std::vector, bp::handle<> sensorsDataPy[N] required template specialization for N=0.
            bp::handle<> qPy(getNumpyReferenceFromEigenVector(q));
            bp::handle<> vPy(getNumpyReferenceFromEigenVector(v));
            std::vector<bp::handle<> > sensorsDataPy(
                {bp::handle<>(getNumpyReferenceFromEigenMatrix(sensorsData))...});
            bp::handle<> uCommandPy(getNumpyReferenceFromEigenVector(uCommand));
            funcPyPtr_(t, qPy, vPy, sensorsDataPy[Is]..., uCommandPy);
        }
    private:
        bp::object funcPyPtr_;
    };

    // ******************************** PyEngine **************************************

    class PyEngine // Composition Over Inheritance
    {
    public:
        // Disable the copy of the class
        PyEngine(void) = delete;
        PyEngine(PyEngine const & simulator) = delete;
        PyEngine & operator = (PyEngine const & other) = delete;

    public:
        PyEngine(Model              & model,
                 AbstractController & controller) :
        model_(&model),
        controller_(&controller),
        engine_()
        {
            // Empty.
        }

        ~PyEngine(void)
        {
            // Empty.
        }

        result_t simulate(vectorN_t     const & x_init,
                          float64_t     const & end_time,
                          callbackFct_t         callbackFct)
        {
            result_t returnCode = result_t::SUCCESS;

            returnCode = engine_.initialize(*model_, *controller_, callbackFct);

            if (returnCode == result_t::SUCCESS)
            {
                returnCode = engine_.simulate(x_init, end_time);
            }

            return returnCode;
        }

        static boost::shared_ptr<PyEngine> pyEngineFactory(Model              & model,
                                                           AbstractController & controller)
        {
            if (pyEnginePtr_.use_count())
            {
                boost::shared_ptr<PyEngine> pyEngineSharedPtr = pyEnginePtr_.lock();
                pyEngineSharedPtr->model_ = &model;
                pyEngineSharedPtr->controller_ = &controller;
                pyEngineSharedPtr->engine_.reset(true);
                pyEngineSharedPtr->controller_->reset(true);
                return pyEngineSharedPtr;
            }
            else
            {
                return boost::shared_ptr<PyEngine>(new PyEngine(model, controller));
            }
        }

    public:
        Model * model_; // Raw pointer to avoid managing its deletion
        AbstractController * controller_;
        Engine engine_;

    private:
        static boost::weak_ptr<PyEngine> pyEnginePtr_;
    };

    boost::weak_ptr<PyEngine> PyEngine::pyEnginePtr_ = boost::weak_ptr<PyEngine>();

    // ***************************** PySensorVisitor ***********************************

    struct PySensorVisitor
        : public bp::def_visitor<PySensorVisitor>
    {
    public:
        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose C++ API through the visitor.
        ///////////////////////////////////////////////////////////////////////////////

        template<class PyClass>
        class PySensorVisit
        {
        public:
            using TSensor = typename PyClass::wrapped_type;

            static void visitAbstract(PyClass& cl)
            {
                cl
                    .def("get_options", &PySensorVisitor::getOptions<TSensor>,
                                        bp::return_value_policy<bp::return_by_value>())
                    .def("set_options", &PySensorVisitor::setOptions<TSensor>)

                    .add_property("name", bp::make_function(&AbstractSensorBase::getName,
                                        bp::return_value_policy<bp::copy_const_reference>()))
                    .add_property("type", bp::make_function(&AbstractSensorBase::getType,
                                        bp::return_value_policy<bp::copy_const_reference>()))
                    .add_property("is_initialized", bp::make_function(&AbstractSensorBase::getIsInitialized,
                                                    bp::return_value_policy<bp::copy_const_reference>()))
                    .add_property("fieldnames", bp::make_function(&AbstractSensorBase::getFieldNames,
                                                bp::return_value_policy<bp::copy_const_reference>()))
                    ;
            }

            template<class Q = TSensor>
            static typename std::enable_if<!std::is_same<Q, AbstractSensorBase>::value, void>::type
            visit(PyClass& cl)
            {
                visitAbstract(cl);

                cl
                    .def("initialize", &TSensor::initialize);
                    ;
            }

            template<class Q = TSensor>
            static typename std::enable_if<std::is_same<Q, AbstractSensorBase>::value, void>::type
            visit(PyClass& cl)
            {
                visitAbstract(cl);
            }
        };

    public:
        template<class PyClass>
        void visit(PyClass& cl) const
        {
            PySensorVisit<PyClass>::visit(cl);
        }

        template<typename TSensor>
        static std::shared_ptr<TSensor> pySensorFactory(void)
        {
            return {nullptr};
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief      Getters and Setters
        ///////////////////////////////////////////////////////////////////////////////

        template<typename TSensor>
        static bp::dict getOptions(TSensor & self)
        {
            bp::dict configPy;
            convertConfigHolderPy(self.getOptions(), configPy);
            return configPy;
        }

        template<typename TSensor>
        static void setOptions(TSensor        & self,
                               bp::dict const & configPy)
        {
            configHolder_t config = self.getOptions();
            loadConfigHolder(configPy, config);
            self.setOptions(config);
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose.
        ///////////////////////////////////////////////////////////////////////////////
        static void expose()
        {
            bp::class_<AbstractSensorBase,
                       boost::shared_ptr<AbstractSensorBase>,
                       boost::noncopyable>("abstract_sensor", bp::no_init)
                .def(PySensorVisitor());
            bp::register_ptr_to_python< std::shared_ptr<AbstractSensorBase> >(); // Required to handle shared_ptr from/to Python

            bp::class_<ImuSensor, bp::bases<AbstractSensorBase>,
                       boost::shared_ptr<ImuSensor>,
                       boost::noncopyable>("imu_sensor", bp::no_init)
                .def(PySensorVisitor());
            bp::register_ptr_to_python< std::shared_ptr<ImuSensor> >();

            bp::class_<ForceSensor, bp::bases<AbstractSensorBase>,
                       boost::shared_ptr<ForceSensor>,
                       boost::noncopyable>("force_sensor", bp::no_init)
                .def(PySensorVisitor());
            bp::register_ptr_to_python< std::shared_ptr<ForceSensor> >();

            bp::class_<EncoderSensor, bp::bases<AbstractSensorBase>,
                       boost::shared_ptr<EncoderSensor>,
                       boost::noncopyable>("encoder_sensor", bp::no_init)
                .def(PySensorVisitor());
            bp::register_ptr_to_python< std::shared_ptr<EncoderSensor> >();
        }
    };

    // ***************************** PyModelVisitor ***********************************

    struct PyModelVisitor
        : public bp::def_visitor<PyModelVisitor>
    {
    public:
        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose C++ API through the visitor.
        ///////////////////////////////////////////////////////////////////////////////
        template<class PyClass>
        void visit(PyClass& cl) const
        {
            cl
                .def("initialize", &PyModelVisitor::initialize,
                                   (bp::arg("self"), "urdf_path", "contacts", "motors", "has_freeflyer"))

                .def("add_imu_sensor", &PyModelVisitor::createAndAddSensor<ImuSensor>,
                                       (bp::arg("self"), "sensor_name", "frame_name"))
                .def("add_force_sensor", &PyModelVisitor::createAndAddSensor<ForceSensor>,
                                         (bp::arg("self"), "sensor_name", "frame_name"))
                .def("add_encoder_sensor", &PyModelVisitor::createAndAddSensor<EncoderSensor>,
                                           (bp::arg("self"), "sensor_name", "motor_name"))
                .def("remove_sensor", &Model::removeSensor,
                                      (bp::arg("self"), "sensor_type", "sensor_name"))
                .def("remove_sensors", &Model::removeSensors,
                                       (bp::arg("self"), "sensorType"))
                .def("get_sensor", &PyModelVisitor::getSensor,
                                   (bp::arg("self"), "sensor_type", "sensor_name"),
                                   bp::return_value_policy<bp::reference_existing_object>())

                .def("get_model_options", &PyModelVisitor::getModelOptions,
                                          bp::return_value_policy<bp::return_by_value>())
                .def("set_model_options", &PyModelVisitor::setModelOptions)
                .def("get_sensors_options", &PyModelVisitor::getSensorsOptions,
                                            bp::return_value_policy<bp::return_by_value>())
                .def("set_sensors_options", &PyModelVisitor::setSensorsOptions)

                .add_property("pinocchio_model", &PyModelVisitor::getPinocchioModel)
                .add_property("frames_names", &PyModelVisitor::getFramesNames)

                .add_property("is_initialized", bp::make_function(&Model::getIsInitialized,
                                                bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("has_freeflyer", bp::make_function(&Model::getHasFreeFlyer,
                                               bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("urdf_path", bp::make_function(&Model::getUrdfPath,
                                               bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("motors_names", bp::make_function(&Model::getMotorsNames,
                                               bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("joints_names", bp::make_function(&Model::getRigidJointsNames,
                                               bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("contact_frames_idx", bp::make_function(&Model::getContactFramesIdx,
                                                    bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("motors_position_idx", bp::make_function(&Model::getMotorsPositionIdx,
                                                     bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("motors_velocity_idx", bp::make_function(&Model::getMotorsVelocityIdx,
                                                     bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("position_fieldnames", bp::make_function(&Model::getPositionFieldNames,
                                                     bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("velocity_fieldnames", bp::make_function(&Model::getVelocityFieldNames,
                                                     bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("acceleration_fieldnames", bp::make_function(&Model::getAccelerationFieldNames,
                                                         bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("motor_torque_fieldnames", bp::make_function(&Model::getMotorTorqueFieldNames,
                                                         bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("nq", bp::make_function(&Model::nq,
                                    bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("nv", bp::make_function(&Model::nv,
                                    bp::return_value_policy<bp::copy_const_reference>()))
                .add_property("nx", bp::make_function(&Model::nx,
                                    bp::return_value_policy<bp::copy_const_reference>()))
                ;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief      Initialize the model
        ///////////////////////////////////////////////////////////////////////////////
        static result_t initialize(Model             & self,
                               std::string const & urdfPath,
                               bp::list    const & contactFramesNamesPy,
                               bp::list    const & motorsNamesPy,
                               bool        const & hasFreeflyer)
        {
            std::vector<std::string> contactFramesNames = listPyToStdVector<std::string>(contactFramesNamesPy);
            std::vector<std::string> motorsNames = listPyToStdVector<std::string>(motorsNamesPy);
            return self.initialize(urdfPath, contactFramesNames, motorsNames, hasFreeflyer);
        }

        template<typename TSensor>
        static result_t createAndAddSensor(Model             & self,
                                           std::string const & sensorName,
                                           std::string const & name)
        {
            result_t returnCode = result_t::SUCCESS;

            std::shared_ptr<TSensor> sensor;
            returnCode = self.addSensor(sensorName, sensor);

            if (returnCode == result_t::SUCCESS)
            {
                returnCode = sensor-> initialize(name);
            }

            return returnCode;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief      Getters and Setters
        ///////////////////////////////////////////////////////////////////////////////

        static AbstractSensorBase * getSensor(Model & self,
                                              std::string const & sensorType,
                                              std::string const & sensorName)
        {
            std::shared_ptr<AbstractSensorBase> sensor;
            self.getSensor(sensorType, sensorName, sensor);
            return sensor.get();
        }

        static pinocchio::Model getPinocchioModel(Model & self)
        {
            return self.pncModel_;
        }

        static std::vector<std::string> getFramesNames(Model & self)
        {
            pinocchio::container::aligned_vector<pinocchio::Frame> frames =
                self.pncModel_.frames;
            std::vector<std::string> framesNames;
            for (pinocchio::Frame const & frame : frames)
            {
                framesNames.push_back(frame.name);
            }
            return framesNames;
        }

        static bp::dict getModelOptions(Model & self)
        {
            bp::dict configModelPy;
            convertConfigHolderPy(self.getOptions(), configModelPy);

            bp::dict configTelemetryPy;
            configHolder_t configTelemetry;
            self.getTelemetryOptions(configTelemetry);
            convertConfigHolderPy(configTelemetry, configTelemetryPy);
            configModelPy["telemetry"] = configTelemetryPy;

            return configModelPy;
        }

        static void setModelOptions(Model          & self,
                                    bp::dict const & configPy)
        {
            configHolder_t configModel = self.getOptions();
            loadConfigHolder(configPy, configModel);
            self.setOptions(configModel);

            configHolder_t configTelemetry;
            self.getTelemetryOptions(configTelemetry);
            loadConfigHolder(bp::extract<bp::dict>(configPy["telemetry"]), configTelemetry);
            self.setTelemetryOptions(configTelemetry);
        }

        static bp::dict getSensorsOptions(Model & self)
        {
            configHolder_t config;
            bp::dict configPy;
            self.getSensorsOptions(config);
            convertConfigHolderPy(config, configPy);

            return configPy;
        }

        static void setSensorsOptions(Model          & self,
                                      bp::dict const & configPy)
        {
            configHolder_t config;
            self.getSensorsOptions(config);
            loadConfigHolder(configPy, config);
            self.setSensorsOptions(config);
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose.
        ///////////////////////////////////////////////////////////////////////////////
        static void expose()
        {
            bp::class_<Model,
                       boost::shared_ptr<Model>,
                       boost::noncopyable>("model")
                .def(PyModelVisitor());
        }
    };


    // ***************************** PyAbstractControllerVisitor ***********************************

    struct PyAbstractControllerVisitor
        : public bp::def_visitor<PyAbstractControllerVisitor>
    {
    public:
        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose C++ API through the visitor.
        ///////////////////////////////////////////////////////////////////////////////
        template<class PyClass>
        void visit(PyClass& cl) const
        {
            cl
                .def("initialize", &AbstractController::initialize,
                                   (bp::arg("self"), "model"))
                .def("register_entry", &PyAbstractControllerVisitor::registerNewEntry,
                                       (bp::arg("self"), "fieldname", "value"))
                .def("register_entry", &PyAbstractControllerVisitor::registerNewVectorEntry)
                .def("remove_entries", &PyAbstractControllerVisitor::removeEntries)
                .def("get_controller_options", &PyAbstractControllerVisitor::getControllerOptions,
                                               bp::return_value_policy<bp::return_by_value>())
                .def("set_controller_options", &PyAbstractControllerVisitor::setControllerOptions)
                ;
        }

        static void registerNewEntry(AbstractController       & self,
                                     std::string        const & fieldName,
                                     PyObject                 * dataPy)
        {
            float64_t const * data = (float64_t *) PyArray_DATA(reinterpret_cast<PyArrayObject *>(dataPy));
            self.registerNewEntry(fieldName, *data);
        }

        static void registerNewVectorEntry(AbstractController       & self,
                                           bp::list           const & fieldNamesPy,
                                           PyObject                 * dataPy) // Const qualifier is not supported by PyArray_DATA anyway
        {
            std::vector<std::string> fieldNames = listPyToStdVector<std::string>(fieldNamesPy);
            Eigen::Map<vectorN_t> data((float64_t *) PyArray_DATA(reinterpret_cast<PyArrayObject *>(dataPy)), fieldNames.size());
            self.registerNewVectorEntry(fieldNames, data);
        }

        static void removeEntries(AbstractController & self)
        {
            self.reset(true);
        }

        static bp::dict getControllerOptions(AbstractController & self)
        {
            bp::dict configPy;
            convertConfigHolderPy(self.getOptions(), configPy);
            return configPy;
        }

        static void setControllerOptions(AbstractController       & self,
                                         bp::dict           const & configPy)
        {
            configHolder_t config = self.getOptions();
            loadConfigHolder(configPy, config);
            self.setOptions(config);
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose.
        ///////////////////////////////////////////////////////////////////////////////
        static void expose()
        {
            bp::class_<AbstractController,
                       boost::shared_ptr<AbstractController>,
                       boost::noncopyable>("abstract_controller", bp::no_init)
                .def(PyAbstractControllerVisitor());
        }
    };

    #ifdef PYTHON_CONTROLLER_FUNCTOR_MAX_SENSOR_TYPES
    static AbstractController * PyControllerFunctorNFactory(bp::object       & commandPy,
                                                            bp::object       & internalDynamicsPy,
                                                            int32_t    const & numSensorTypes)
    {
        /* 'co_varnames' is used along with 'co_argcount' to avoid accounting for
           the 'self' argument in case of class method handle.
           Note that it does not support Python *args and **kwargs. In such a case,
           specify 'numSensorTypes' argument instead. */

        int32_t N;
        if (numSensorTypes < 0)
        {
            std::string argsFirstPy = boost::python::extract<std::string>(
                commandPy.attr("func_code").attr("co_varnames")[0]);
            std::size_t argsNumPy = boost::python::extract<std::size_t>(
                    commandPy.attr("func_code").attr("co_argcount"));
            N = argsNumPy - 4 - (argsFirstPy == std::string("self"));
        }
        else
        {
            N = numSensorTypes;
        }
        assert(N <= PYTHON_CONTROLLER_FUNCTOR_MAX_SENSOR_TYPES || N < 0);
        if (N < 0)
        {
            std::cout << "The functors in argument are ill-defined." << std::endl;
        }
        if (N > PYTHON_CONTROLLER_FUNCTOR_MAX_SENSOR_TYPES)
        {
            N = PYTHON_CONTROLLER_FUNCTOR_MAX_SENSOR_TYPES;
            std::cout << "Maximum number of sensors exceeded by 'ControllerFunctor' Python binding." << std::endl;
        }

        #define TO_SEQ_ELEM(z, n, data) (n)
        #define MAKE_INTEGER_SEQUENCE(n) BOOST_PP_REPEAT(n, TO_SEQ_ELEM, )
        #define RANGE MAKE_INTEGER_SEQUENCE(PYTHON_CONTROLLER_FUNCTOR_MAX_SENSOR_TYPES)
        #define MACRO(r, data, p) \
            if (p == N) \
            { \
                using T = ControllerFctWrapperN<p>; \
                T commandFct(commandPy); \
                T internalDynamicsFct(internalDynamicsPy); \
                return new ControllerFunctor<T, T>(std::move(commandFct), std::move(internalDynamicsFct)); \
            }
        BOOST_PP_SEQ_FOR_EACH(MACRO, _, RANGE)
        #undef MACRO
        #undef RANGE

        // Fallback in case the number of sensors N is out of range
        using T_MAX = ControllerFctWrapperN<PYTHON_CONTROLLER_FUNCTOR_MAX_SENSOR_TYPES>;
        T_MAX commandFct(commandPy);
        T_MAX internalDynamicsFct(internalDynamicsPy);
        return new ControllerFunctor<T_MAX, T_MAX>(std::move(commandFct), std::move(internalDynamicsFct));
    }
    #endif  // PYTHON_CONTROLLER_FUNCTOR_MAX_SENSOR_TYPES

    // ***************************** PyEngineVisitor ***********************************

    struct PyEngineVisitor
        : public bp::def_visitor<PyEngineVisitor>
    {
    public:
        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose C++ API through the visitor.
        ///////////////////////////////////////////////////////////////////////////////
        template<class PyClass>
        void visit(PyClass& cl) const
        {
            cl
                .def("__init__", bp::make_constructor(&PyEngine::pyEngineFactory,
                                 bp::default_call_policies(),
                                 (bp::arg("model"), "controller")))
                .def("run", &PyEngineVisitor::simulate,
                            (bp::arg("self"), "x_init", "end_time"))
                .def("run", &PyEngineVisitor::simulateWithCallback,
                            (bp::arg("self"), "x_init", "end_time", "callback_handle"))
                .def("get_log", &PyEngineVisitor::getLog,
                                bp::return_value_policy<bp::return_by_value>())
                .def("write_log", &PyEngineVisitor::writeLog,
                                 (bp::arg("self"), "filename", bp::arg("isModeBinary")=false))
                .def("register_force_impulse", &PyEngineVisitor::registerForceImpulse,
                                               (bp::arg("self"), "frame_name", "t", "dt", "F"))
                .def("register_force_profile", &PyEngineVisitor::registerForceProfile,
                                               (bp::arg("self"), "frame_name", "force_handle"))
                .def("get_engine_options", &PyEngineVisitor::getEngineOptions,
                                           bp::return_value_policy<bp::return_by_value>())
                .def("set_engine_options", &PyEngineVisitor::setEngineOptions)
                ;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief      Run the simulation
        ///////////////////////////////////////////////////////////////////////////////
        static void simulate(PyEngine         & self,
                             vectorN_t  const & x_init,
                             float64_t  const & end_time)
        {
            callbackFct_t callbackFct = [](float64_t const & t,
                                           vectorN_t const & x) -> bool
            {
                return true;
            };
            self.simulate(x_init, end_time, callbackFct);
        }

        static void simulateWithCallback(PyEngine         & self,
                                         vectorN_t  const & x_init,
                                         float64_t  const & end_time,
                                         bp::object const & callbackPy)
        {
            TimeStateFctPyWrapper<bool> callbackFct(callbackPy);
            self.simulate(x_init, end_time, std::move(callbackFct));
        }

        static void writeLog(PyEngine          & self,
                             std::string const & filename,
                             bool        const & isModeBinary)
        {
            if (isModeBinary)
            {
                self.engine_.writeLogBinary(filename);
            }
            else
            {
                self.engine_.writeLogTxt(filename);
            }
        }

        static void registerForceImpulse(PyEngine          & self,
                                         std::string const & frameName,
                                         float64_t   const & t,
                                         float64_t   const & dt,
                                         vector3_t   const & F)
        {
            self.engine_.registerForceImpulse(frameName, t, dt, F);
        }

        static void registerForceProfile(PyEngine          & self,
                                         std::string const & frameName,
                                         bp::object  const & forcePy)
        {
            TimeStateFctPyWrapper<vector3_t> forceFct(forcePy);
            self.engine_.registerForceProfile(frameName, std::move(forceFct));
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief      Getters and Setters
        ///////////////////////////////////////////////////////////////////////////////

        static bp::tuple getLog(PyEngine & self)
        {
            std::vector<std::string> header;
            matrixN_t log;
            self.engine_.getLogData(header, log);
            return bp::make_tuple(header, log);
        }

        static bp::dict getEngineOptions(PyEngine & self)
        {
            bp::dict configPy;
            convertConfigHolderPy(self.engine_.getOptions(), configPy);
            return configPy;
        }

        static void setEngineOptions(PyEngine       & self,
                                     bp::dict const & configPy)
        {
            configHolder_t config = self.engine_.getOptions();
            loadConfigHolder(configPy, config);
            self.engine_.setOptions(config);
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// \brief Expose.
        ///////////////////////////////////////////////////////////////////////////////
        static void expose()
        {
            bp::class_<PyEngine,
                       boost::shared_ptr<PyEngine>,
                       boost::noncopyable>("simulator", bp::no_init)
                .def(PyEngineVisitor());
        }
    };
}  // End of namespace python.
}  // End of namespace jiminy.

#endif  // SIMULATOR_PYTHON_H