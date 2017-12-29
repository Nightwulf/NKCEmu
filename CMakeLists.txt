project(nkcemu)
cmake_minimum_required(VERSION 2.8)

add_executable(nkcemu sim1.c sim2.c sim3.c sim4.c sim5.c sim6.c sim7.c simctl.c simfun.c simglb.c simint.c gdp64.c iosim.c disas.c cas.c nkcemu.c)

target_link_libraries(nkcemu SDL)

install(TARGETS nkcemu RUNTIME DESTINATION bin)
