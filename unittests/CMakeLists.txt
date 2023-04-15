find_package(Catch2 REQUIRED)

add_executable(tests
    defaults.hpp
    jsonutils.cpp
    jsonutils.hpp
    main.cpp
    mockedmodbuscontext.cpp
    mockedmodbuscontext.hpp
    mockedmqttimpl.cpp
    mockedmqttimpl.hpp
    mockedserver.hpp
    # tests
    mqtt_rpc_tests.cpp
)

target_link_libraries(tests 
    modmqttsrv
    mosquittopp 
    mosquitto 
    Catch2::Catch2
    ${YAML_CPP_LIBRARIES} 
    ${Boost_LIBRARIES} 
    ${LIBMODBUS_LIBRARIES} 
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}    
)
