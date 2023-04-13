#include "mockedmodbuscontext.hpp"

#include "libmodmqttsrv/modbus_messages.hpp"
#include "libmodmqttsrv/modbus_context.hpp"
#include "catch2/catch.hpp"

#include <thread>
#include <iostream>

const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveReadTime = std::chrono::milliseconds(5);
const std::chrono::milliseconds MockedModbusContext::sDefaultSlaveWriteTime = std::chrono::milliseconds(10);

void
MockedModbusContext::Slave::write(const modmqttd::MsgRegisterValue& msg, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mWriteTime);
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusWriteException(std::string("write fn ") + std::to_string(msg.mRegisterAddress) + " failed");
        }
        if (hasError(msg.mRegisterAddress, msg.mRegisterType)) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("register write fn ") + std::to_string(msg.mRegisterAddress) + " failed");
        }
    }
    int regAddress = msg.mRegisterAddress;
    for (auto it = msg.mValues.begin(); it != msg.mValues.end(); ++it, ++regAddress) {
        switch(msg.mRegisterType) {
            case modmqttd::RegisterType::COIL:
                mCoil[regAddress].mValue = *it == 1;
            break;
            case modmqttd::RegisterType::BIT:
                mBit[regAddress].mValue = *it == 1;
            break;
            case modmqttd::RegisterType::HOLDING:
                mHolding[regAddress].mValue = *it;
            break;
            case modmqttd::RegisterType::INPUT:
                mInput[regAddress].mValue = *it;
            break;
            default:
                throw modmqttd::ModbusWriteException(std::string("Cannot write, unknown register type ") + std::to_string(msg.mRegisterType));
        };
    }
}

uint16_t
MockedModbusContext::Slave::read(int registerAddress, modmqttd::RegisterType registerType, bool internalOperation) {
    if (!internalOperation) {
        std::this_thread::sleep_for(mReadTime);
        if (mDisconnected) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("read fn ") + std::to_string(registerAddress) + " failed");
        }
        if (hasError(registerAddress, registerType)) {
            errno = EIO;
            throw modmqttd::ModbusReadException(std::string("register read fn ") + std::to_string(registerAddress) + " failed");
        }
    }
    switch(registerType) {
        case modmqttd::RegisterType::COIL:
            return readRegister(mCoil, registerAddress);
        break;
        case modmqttd::RegisterType::HOLDING:
            return readRegister(mHolding, registerAddress);
        break;
        case modmqttd::RegisterType::INPUT:
            return readRegister(mInput, registerAddress);
        break;
        case modmqttd::RegisterType::BIT:
            return readRegister(mBit, registerAddress);
        break;
        default:
            throw modmqttd::ModbusReadException(std::string("Cannot read, unknown register type ") + std::to_string(registerType));
    };
}

bool
MockedModbusContext::Slave::hasError(int regNum, modmqttd::RegisterType regType) const {
    switch(regType) {
        case modmqttd::RegisterType::COIL:
            return hasError(mCoil, regNum);
        break;
        case modmqttd::RegisterType::HOLDING:
            return hasError(mHolding, regNum);
        break;
        case modmqttd::RegisterType::INPUT:
            return hasError(mInput, regNum);
        break;
        case modmqttd::RegisterType::BIT:
            return hasError(mBit, regNum);
        break;
        default:
            throw modmqttd::ModbusReadException(
                std::string("Cannot check for error, unknown register type ")
                + std::to_string(regType)
            );
    };
}

void
MockedModbusContext::Slave::setError(int regNum, modmqttd::RegisterType regType, bool pFlag) {
    switch(regType) {
        case modmqttd::RegisterType::COIL:
            mCoil[regNum].mError = pFlag;
        break;
        case modmqttd::RegisterType::BIT:
            mBit[regNum].mError = pFlag;
        break;
        case modmqttd::RegisterType::HOLDING:
            mHolding[regNum].mError = pFlag;
        break;
        case modmqttd::RegisterType::INPUT:
            mInput[regNum].mError = pFlag;
        break;
        default:
            throw modmqttd::ModbusReadException(std::string("Cannot set error, unknown register type ") + std::to_string(regType));
    };
}

uint16_t
MockedModbusContext::Slave::readRegister(std::map<int, MockedModbusContext::Slave::RegData>& table, int num) {
    auto it = table.find(num);
    if (it == table.end())
        return 0;
    return it->second.mValue;
}

bool
MockedModbusContext::Slave::hasError(const std::map<int, MockedModbusContext::Slave::RegData>& table, int num) const {
    auto it = table.find(num);
    if (it == table.end())
        return false;
    return it->second.mError;
}

uint16_t
MockedModbusContext::readModbusRegister(int slaveId, const modmqttd::BaseRegisterInfo& regData) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(slaveId);
    uint16_t ret = it->second.read(regData.mRegisterAddress, regData.mRegisterType, mInternalOperation);
    if (mInternalOperation)
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << regData.mRegisterAddress
            << " READ: " << ret;

    mInternalOperation = false;
    return ret;
}

static std::string toStr(const std::vector<uint16_t>& vec) {
    std::stringstream str;
    for (auto it = vec.begin(); it != vec.end(); ++it) {
        str << (*it) << ", ";
    }
    return str.str();
}

std::vector<uint16_t>
MockedModbusContext::readModbusRegisters(const modmqttd::MsgRegisterReadRpc& msg) {
    std::vector<uint16_t> ret;
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(msg.mSlaveId);
    for (int i = 0; i < msg.mSize; i++) {
        ret.push_back(it->second.read(msg.mRegisterAddress + i, msg.mRegisterType, mInternalOperation));
    }
    if (mInternalOperation)
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << msg.mRegisterAddress << "[" << msg.mSize << "]"
            << " READ: " << toStr(ret);

    mInternalOperation = false;
    return ret;
}

void
MockedModbusContext::init(const modmqttd::ModbusNetworkConfig& config) {
    mNetworkName = config.mName;
}

void
MockedModbusContext::writeModbusRegister(const modmqttd::MsgRegisterValue& msg) {
    std::unique_lock<std::mutex> lck(mMutex);
    std::map<int, Slave>::iterator it = findOrCreateSlave(msg.mSlaveId);

    if (mInternalOperation) {
        BOOST_LOG_SEV(log, modmqttd::Log::info) << "MODBUS: " << mNetworkName
            << "." << it->second.mId << "." << msg.mRegisterAddress
            << " WRITE: [" << toStr(msg.mValues) << "]";
    }
    it->second.write(msg, mInternalOperation);
    mInternalOperation = false;
}

std::map<int, MockedModbusContext::Slave>::iterator
MockedModbusContext::findOrCreateSlave(int id) {
    std::map<int, Slave>::iterator it = mSlaves.find(id);
    if (it == mSlaves.end()) {
        Slave s(id);
        mSlaves[id] = s;
        it = mSlaves.find(id);
    }
    return it;
}

MockedModbusContext::Slave&
MockedModbusContext::getSlave(int slaveId) {
    return findOrCreateSlave(slaveId)->second;
}

std::shared_ptr<MockedModbusContext>
MockedModbusFactory::getOrCreateContext(const char* network) {
    auto it = mModbusNetworks.find(network);
    std::shared_ptr<MockedModbusContext> ctx;
    if (it == mModbusNetworks.end()) {
        ctx.reset(new MockedModbusContext());
        modmqttd::ModbusNetworkConfig c;
        c.mName = network;
        ctx->init(c);
        mModbusNetworks[network] = ctx;
    } else {
        ctx = it->second;
    }
    return ctx;
}

uint16_t
MockedModbusFactory::getModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    modmqttd::BaseRegisterInfo msg(regNum, regtype);
    ctx->mInternalOperation = true;
    return ctx->readModbusRegister(slaveId, msg);
}

void
MockedModbusFactory::setModbusRegisterValue(const char* network, int slaveId, int regNum, modmqttd::RegisterType regtype, uint16_t val) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    modmqttd::MsgRegisterValue msg(slaveId, regtype, regNum, val);
    ctx->mInternalOperation = true;
    ctx->writeModbusRegister(msg);
}

void
MockedModbusFactory::setModbusRegisterReadError(const char* network, int slaveId, int regNum, modmqttd::RegisterType regType) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    MockedModbusContext::Slave& s(ctx->getSlave(slaveId));
    s.setError(regNum, regType);
}


void
MockedModbusFactory::disconnectModbusSlave(const char* network, int slaveId) {
    std::shared_ptr<MockedModbusContext> ctx = getOrCreateContext(network);
    ctx->getSlave(slaveId).setDisconnected();
}


