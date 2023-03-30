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

    server.waitForPublish("test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "32");

    server.waitForPublish("test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "32");

    server.stop();
}

TEST_CASE ("Coil register valid write") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();

    server.waitForPublish("test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "1");

    server.waitForPublish("test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}

TEST_CASE ("Mqtt invalid value should not crash server") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::COIL, 0);
    server.start();


    server.waitForPublish("test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);

    server.publish("test_switch/set", "hello, world!");
    server.publish("test_switch/set", "1");
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "1");

    server.stop();
}

TEST_CASE ("Mqtt binary range value should not be permitted if not configured") {
    MockedModMqttServerThread server(config);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.start();

    server.waitForPublish("test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);

    server.publishBuffer("test_switch/set", { 43, 44 });
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "42");

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
    - topic: some/subpath/test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
          range: true
      state:
        register: tcptest.1.2
        register_type: holding
)";
TEST_CASE ("Mqtt binary range value should work if configured") {
    MockedModMqttServerThread server(config_range);
    server.setModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING, 42);
    server.start();

    server.waitForPublish("test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/availability") == "1");
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);

    server.publishBuffer("test_switch/set", { 43, 44 });
    server.waitForPublish("test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("test_switch/state") == "43");
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 2, modmqttd::RegisterType::HOLDING) == 43);
    REQUIRE(server.getModbusRegisterValue("tcptest", 1, 3, modmqttd::RegisterType::HOLDING) == 44);

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

    server.waitForPublish("some/subpath/test_switch/availability", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("some/subpath/test_switch/availability") == "1");
    server.waitForPublish("some/subpath/test_switch/state", REGWAIT_MSEC);

    server.publish("some/subpath/test_switch/set", "32");

    server.waitForPublish("some/subpath/test_switch/state", REGWAIT_MSEC);
    REQUIRE(server.mqttValue("some/subpath/test_switch/state") == "32");

    server.stop();
}
