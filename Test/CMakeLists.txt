# CmakeLists.txt in Test

INCLUDE_DIRECTORIES(${Cornucopia_SOURCE_DIR}/Cornucopia)
INCLUDE_DIRECTORIES(${Cornucopia_SOURCE_DIR}/DemoUI)

FILE(GLOB Test_CPP "*.cpp")
FILE(GLOB Test_H "*.h")

LIST(APPEND Test_Sources ${Test_CPP} ${Test_H})

ADD_EXECUTABLE(Test ${Test_Sources})

TARGET_LINK_LIBRARIES(Test Cornucopia ${QT_LIBRARIES})
