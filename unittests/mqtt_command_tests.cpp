#include "catch2/catch.hpp"
#include "mockedserver.hpp"
#include "defaults.hpp"

static const std::string config = R"(
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
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
      state:
        register: tcptest.1.2
        register_type: holding
)";

TEST_CASE ("Holding register valid write") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    waitForPublish(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "32");

    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "32");

    server.stop();
}

TEST_CASE ("Coil register valid write") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();

    waitForPublish(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}

TEST_CASE ("Mqtt invalid value should not crash server") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();

    waitForPublish(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.publish("test_switch/set", "hello, world!");
    REQUIRE(server.checkForPublish("test_switch/state", REGWAIT_MSEC) == false);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}

TEST_CASE ("Mqtt binary range value should not be permitted if not configured") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.start();

    waitForPublish(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", std::vector<uint8_t>({ 43, 44 }));
    REQUIRE(server.checkForPublish("test_switch/state", REGWAIT_MSEC) == false);

    server.stop();
}

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
      commands:
        - name: range
          register: tcptest.1.2
          register_type: holding
          payload_type: binary
          size: 2
      state:
        register: tcptest.1.2
        register_type: holding
)";
TEST_CASE ("Mqtt binary range write should work if configured") {
    MockedModMqttServerThread server(config_range);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.start();

    waitForPublish(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);

    // In little endian format
    server.publish("test_switch/range", std::vector<uint8_t>({ 43, 0, 44, 1 }));
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);
    // For compatibility, the state is still one register
    REQUIRE(server.mqttValue("test_switch/state") == "43");
    // But the writing impacted two registers
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 43);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 256 + 44);

    // Do it again to test range limit, rejected because out of bounds
    server.publish("test_switch/range", std::vector<uint8_t>({ 53, 0, 54, 0, 55, 0 }));
    REQUIRE(server.checkForPublish("test_switch/state", REGWAIT_MSEC) == false);
    REQUIRE(server.mqttValue("test_switch/state") == "43");

    server.stop();
}

TEST_CASE ("Mqtt binary range read via RPC should work if configured") {
    MockedModMqttServerThread server(config_range);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.setModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING, 43);
    server.start();

    waitForPublish(server, "test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    waitForPublish(server, "test_switch/state", REGWAIT_MSEC);

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

    server.stop();
}

static const std::string config_subpath = R"(
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
    - topic: some/subpath/test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
      state:
        register: tcptest.1.2
        register_type: holding
)";
TEST_CASE ("Holding register valid write to subpath topic") {
    MockedModMqttServerThread server(config_subpath);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 0);
    server.start();

    waitForPublish(server, "some/subpath/test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("some/subpath/test_switch/availability") == "1");
    waitForPublish(server, "some/subpath/test_switch/state", REGWAIT_MSEC);

    server.publish("some/subpath/test_switch/set", "32");

    waitForPublish(server, "some/subpath/test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("some/subpath/test_switch/state") == "32");

    server.stop();
}
