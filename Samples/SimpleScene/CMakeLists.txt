add_executable(SimpleScene Main.cpp ../Common/Framework.cpp)
target_link_libraries(SimpleScene AnKi)

installExecutable(SimpleScene)
