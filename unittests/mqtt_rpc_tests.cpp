#include "catch2/catch.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

static const std::string config_range = R"(
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_switch
      rpc:
        - name: range
          register: tcptest.1.2
          register_type: holding
          payload_type: binary
          size: 2
)";
TEST_CASE ("Mqtt binary range write should work if configured") {
    MockedModMqttServerThread server(config_range);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.start();

    // In little endian format
    server.publish("test_switch/range", std::vector<uint8_t>({ 43, 0, 44, 1 }));
    // But the writing impacted two registers
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 43);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 256 + 44);

    // Do it again to test range limit, rejected because out of bounds
    server.publish("test_switch/range", std::vector<uint8_t>({ 53, 0, 54, 0, 55, 0 }));
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 43);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 256 + 44);

    server.stop();
}

TEST_CASE ("Mqtt binary range read via RPC should work if configured") {
    MockedModMqttServerThread server(config_range);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 43);
    server.start();

    // In little endian format
    modmqttd::MqttPublishProps props;
    props.mCorrelationData = { 1, 2, 3, 4 };
    props.mPayloadType = modmqttd::MqttPublishPayloadType::BINARY;
    props.mResponseTopic = "test_switch/read_back";
    server.publish("test_switch/range", { }, props);

    waitForPublish(server, "test_switch/read_back", REGWAIT_MSEC);
    REQUIRE(server.mqttBinaryValue("test_switch/read_back") == std::vector<uint8_t>({ 42, 0, 43, 0 }));
    REQUIRE(server.mqttValueProps("test_switch/read_back").mCorrelationData == std::vector<uint8_t>({ 1, 2, 3, 4 }));
    REQUIRE(server.mqttValueProps("test_switch/read_back").mPayloadType == modmqttd::MqttPublishPayloadType::BINARY);

    // In little endian format
    server.disconnectModbusSlave("tcptest", 1);

    props.mCorrelationData = { 11, 12 };
    props.mPayloadType = modmqttd::MqttPublishPayloadType::BINARY;
    props.mResponseTopic = "test_switch/read_back";
    server.publish("test_switch/range", { }, props);

    waitForPublish(server, "test_switch/read_back", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/read_back") == "libmodbus: read fn 2 failed: Input/output error");
    REQUIRE(server.mqttValueProps("test_switch/read_back").mCorrelationData == std::vector<uint8_t>({ 11, 12 }));
    REQUIRE(server.mqttValueProps("test_switch/read_back").mPayloadType == modmqttd::MqttPublishPayloadType::STRING);

    // Check that availability and polling are not happening. Wait at least the global configured
    // refresh time
    //wait(100ms);
    REQUIRE(server.getModbusRegisterReadCount("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 1);
    REQUIRE(server.getModbusRegisterReadCount("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 1);
    server.stop();
}
