#pragma once

#include <inttypes.h>
#include <memory>

namespace modmqttd {

class BaseRegisterInfo;
class MsgRegisterValue;
class MsgRegisterRangeValues;
class ModbusNetworkConfig;

/**
    Abstract base class for modbus communication library implementation
*/
class IModbusContext {
    public:
        virtual void init(const ModbusNetworkConfig& config) = 0;
        virtual void connect() = 0;
        virtual bool isConnected() const = 0;
        virtual void disconnect() = 0;
        virtual uint16_t readModbusRegister(int slaveId, const BaseRegisterInfo& regData) = 0;
        virtual std::vector<uint16_t> readModbusRegisters(int slaveId, const BaseRegisterInfo& regData, int count) = 0;
        virtual void writeModbusRegister(const MsgRegisterValue& msg) = 0;
        virtual void writeModbusRegisters(const MsgRegisterRangeValues& msg) = 0;
        virtual ~IModbusContext() {};
};

class IModbusFactory {
    public:
        virtual std::shared_ptr<IModbusContext> getContext(const std::string& networkName) = 0;
        virtual ~IModbusFactory() {};
};

}
