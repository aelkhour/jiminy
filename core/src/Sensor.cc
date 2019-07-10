#include <algorithm>

#include "pinocchio/multibody/model.hpp"
#include "pinocchio/algorithm/frames.hpp"

#include "exo_simu/core/Model.h"
#include "exo_simu/core/Sensor.h"


namespace exo_simu
{
    // ===================== ImuSensor =========================

    template<>
    std::string const AbstractSensorTpl<ImuSensor>::type_("ImuSensor");
    template<>
    uint32_t const AbstractSensorTpl<ImuSensor>::sizeOf_(7);

    ImuSensor::ImuSensor(Model                               const & model,
                         std::shared_ptr<SensorDataHolder_t> const & dataHolder,
                         std::string                         const & name) :
    AbstractSensorTpl(model, dataHolder, name),
    imuSensorOptions_(nullptr),
    frameIdx_()
    {
        setOptions(getDefaultOptions());
    }

    ImuSensor::~ImuSensor(void)
    {
        // Empty.
    }

    void ImuSensor::initialize(int32_t const & frameIdx)
    {
        frameIdx_ = frameIdx;
        isInitialized_ = true;
    }

    void ImuSensor::setOptions(configHolder_t const & sensorOptions)
    {
        sensorOptionsHolder_ = sensorOptions;
        imuSensorOptions_ = std::make_shared<imuSensorOptions_t const>(sensorOptionsHolder_);
    }

    int32_t ImuSensor::getFrameIdx(void) const
    {
        return frameIdx_;
    }

    result_t ImuSensor::set(float64_t const & t,
                            vectorN_t const & q,
                            vectorN_t const & v,
                            vectorN_t const & a,
                            vectorN_t const & u)
    {
        result_t returnCode = result_t::SUCCESS;

        if (!isInitialized_)
        {
            std::cout << "Error - ImuSensor::set - Sensor not initialized. Impossible to set sensor data." << std::endl;
            returnCode = result_t::ERROR_INIT_FAILED;
        }

        if (returnCode == result_t::SUCCESS)
        {
            Eigen::Matrix4d const tformIMU = model_->pncData_.oMf[frameIdx_].toHomogeneousMatrix();
            Eigen::Matrix3d const rotIMU = tformIMU.topLeftCorner<3,3>();
            quaternion_t const quatIMU(rotIMU); // Convert a rotation matrix to a quaternion
            data().head(4) = quatIMU.coeffs(); // (x,y,z,w)
            pinocchio::Motion motionIMU = pinocchio::getFrameVelocity(model_->pncModel_, model_->pncData_, frameIdx_);
            Eigen::Vector3d omegaIMU = motionIMU.angular();
            data().tail(3) = omegaIMU;
        }

        return returnCode;
    }

    // ===================== ForceSensor =========================

    template<>
    std::string const AbstractSensorTpl<ForceSensor>::type_("ForceSensor");
    template<>
    uint32_t const AbstractSensorTpl<ForceSensor>::sizeOf_(3);

    ForceSensor::ForceSensor(Model                               const & model,
                             std::shared_ptr<SensorDataHolder_t> const & dataHolder,
                             std::string                         const & name) :
    AbstractSensorTpl(model, dataHolder, name),
    forceSensorOptions_(nullptr),
    frameIdx_()
    {
        setOptions(getDefaultOptions());
    }

    ForceSensor::~ForceSensor(void)
    {
        // Empty.
    }

    void ForceSensor::initialize(int32_t const & frameIdx)
    {
        frameIdx_ = frameIdx;
        isInitialized_ = true;
    }

    void ForceSensor::setOptions(configHolder_t const & sensorOptions)
    {
        sensorOptionsHolder_ = sensorOptions;
        forceSensorOptions_ = std::make_shared<forceSensorOptions_t const>(sensorOptionsHolder_);
    }

    int32_t ForceSensor::getFrameIdx(void) const
    {
        return frameIdx_;
    }

    result_t ForceSensor::set(float64_t const & t,
                              vectorN_t const & q,
                              vectorN_t const & v,
                              vectorN_t const & a,
                              vectorN_t const & u)
    {
        result_t returnCode = result_t::SUCCESS;

        if (!isInitialized_)
        {
            std::cout << "Error - ForceSensor::set - Sensor not initialized. Impossible to set sensor data." << std::endl;
            returnCode = result_t::ERROR_INIT_FAILED;
        }

        if (returnCode == result_t::SUCCESS)
        {
            std::vector<int32_t> const & contactFramesIdx = model_->getContactFramesIdx();
            std::vector<int32_t>::const_iterator it = std::find(contactFramesIdx.begin(), contactFramesIdx.end(), frameIdx_);
            data() = model_->contactForces_[std::distance(contactFramesIdx.begin(), it)].linear();
        }

        return returnCode;
    }

    // ===================== EncoderSensor =========================

    template<>
    std::string const AbstractSensorTpl<EncoderSensor>::type_("EncoderSensor");
    template<>
    uint32_t const AbstractSensorTpl<EncoderSensor>::sizeOf_(2);

    EncoderSensor::EncoderSensor(Model                               const & model,
                                 std::shared_ptr<SensorDataHolder_t> const & dataHolder,
                                 std::string                         const & name) :
    AbstractSensorTpl(model, dataHolder, name),
    encoderSensorOptions_(nullptr),
    jointPositionIdx_(),
    jointVelocityIdx_()
    {
        setOptions(getDefaultOptions());
    }

    EncoderSensor::~EncoderSensor(void)
    {
        // Empty.
    }

    void EncoderSensor::initialize(int32_t const & jointPositionIdx,
                                   int32_t const & jointVelocityIdx)
    {
        jointPositionIdx_ = jointPositionIdx;
        jointVelocityIdx_ = jointVelocityIdx;
        isInitialized_ = true;
    }

    void EncoderSensor::setOptions(configHolder_t const & sensorOptions)
    {
        sensorOptionsHolder_ = sensorOptions;
        encoderSensorOptions_ = std::make_shared<encoderSensorOptions_t const>(sensorOptionsHolder_);
    }

    int32_t EncoderSensor::getJointPositionIdx(void) const
    {
        return jointPositionIdx_;
    }

    int32_t EncoderSensor::getJointVelocityIdx(void) const
    {
        return jointVelocityIdx_;
    }

    result_t EncoderSensor::set(float64_t const & t,
                                vectorN_t const & q,
                                vectorN_t const & v,
                                vectorN_t const & a,
                                vectorN_t const & u)
    {
        result_t returnCode = result_t::SUCCESS;

        if (!isInitialized_)
        {
            std::cout << "Error - EncoderSensor::set - Sensor not initialized. Impossible to set sensor data." << std::endl;
            returnCode = result_t::ERROR_INIT_FAILED;
        }

        if (returnCode == result_t::SUCCESS)
        {
            data().head(1) = q.segment<1>(jointPositionIdx_);
            data().tail(1) = v.segment<1>(jointVelocityIdx_);
        }

        return returnCode;
    }
}