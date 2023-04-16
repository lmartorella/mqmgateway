#pragma once

#include <string>
#include <map>
#include <chrono>
#include <vector>

#include "modbus_types.hpp"

namespace modmqttd {

/**
 * MQTT declared payload type by the topic publisher
 * */
enum MqttPublishPayloadType {
    UNSPECIFIED = 0,
    STRING = 1,
    BINARY = 2
};

/**
 * Optional properties for MQTT publications
 * */
class MqttPublishProps {
    public:
        MqttPublishPayloadType mPayloadType = MqttPublishPayloadType::UNSPECIFIED;
        // Used in MQTT request-response
        std::string mResponseTopic;
        std::vector<uint8_t> mCorrelationData;
};

class MsgMqttCommand {
    public:
        int mSlave;
        int mRegister;
        RegisterType mRegisterType;
        int16_t mData;
};

class MsgRegisterMessageBase {
    public:
        MsgRegisterMessageBase(int slaveId, RegisterType regType, int registerAddress)
            : mSlaveId(slaveId), mRegisterType(regType), mRegisterAddress(registerAddress) {}
        int mSlaveId;
        RegisterType mRegisterType;
        int mRegisterAddress;
};

class MsgRegisterValue : public MsgRegisterMessageBase {
    public:
        MsgRegisterValue(int slaveId, RegisterType regType, int registerAddress, uint16_t value)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress) {
                mValues.push_back(value);
              }
        MsgRegisterValue(int slaveId, RegisterType regType, int registerAddress, const std::vector<uint16_t>& values)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress),
              mValues(values) {}
        std::vector<uint16_t> mValues;
};

class MsgRegisterReadRemoteCall : public MsgRegisterMessageBase {
    public:
        MsgRegisterReadRemoteCall(int slaveId, RegisterType regType, int registerAddress, int size, const MqttPublishProps& responseProps)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress), mSize(size), mResponseProps(responseProps) { }
        int mSize;
        MqttPublishProps mResponseProps;
};

/**
 * Sent back to reset availability
 * */
class MsgRegisterReadFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterReadFailed(int slaveId, RegisterType regType, int registerAddress)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress)
        {}
};

/**
 * Sent back to reset availability
 * */
class MsgRegisterWriteFailed : public MsgRegisterMessageBase {
    public:
        MsgRegisterWriteFailed(int slaveId, RegisterType regType, int registerAddress)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress)
        {}
};

/**
 * Sent back to close the Remote Call
 * */
class MsgRegisterRemoteCallResponseBase : public MsgRegisterMessageBase {
    public:
        MsgRegisterRemoteCallResponseBase(int slaveId, RegisterType regType, int registerAddress, const MqttPublishProps& props)
            : MsgRegisterMessageBase(slaveId, regType, registerAddress), mProps(props)
        {}
    MqttPublishProps mProps;
};

class MsgRegisterRemoteCallResponse : public MsgRegisterRemoteCallResponseBase {
    public:
        MsgRegisterRemoteCallResponse(int slaveId, RegisterType regType, int registerAddress, const MqttPublishProps& responseProps, const std::vector<uint16_t>& data)
            : MsgRegisterRemoteCallResponseBase(slaveId, regType, registerAddress, responseProps), mData(data)
        { }
    std::vector<uint16_t> mData;
};

class MsgRegisterRemoteCallError : public MsgRegisterRemoteCallResponseBase {
    public:
        MsgRegisterRemoteCallError(int slaveId, RegisterType regType, int registerAddress, const MqttPublishProps& responseProps, const std::exception& exc)
            : MsgRegisterRemoteCallResponseBase(slaveId, regType, registerAddress, responseProps), mError(exc.what())
        { }
    std::string mError;
};

class MsgRegisterPoll {
    public:
        int mSlaveId;
        int mRegister;
        RegisterType mRegisterType;
        int mRefreshMsec;
};

class MsgRegisterPollSpecification {
    public:
        MsgRegisterPollSpecification(const std::string& networkName) : mNetworkName(networkName) {}
        std::string mNetworkName;
        std::vector<MsgRegisterPoll> mRegisters;
};

class MsgModbusNetworkState {
    public:
        MsgModbusNetworkState(const std::string& networkName, bool isUp)
            : mNetworkName(networkName), mIsUp(isUp)
        {}
        bool mIsUp;
        std::string mNetworkName;
};

class MsgMqttNetworkState {
    public:
        MsgMqttNetworkState(bool isUp)
            : mIsUp(isUp)
        {}
        bool mIsUp;
};

class EndWorkMessage {
    // no fields here, thread will check type of message and exit
};

}
