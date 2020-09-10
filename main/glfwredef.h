#pragma once

#define glfwInit _cdecl glfwInit
#define glfwWindowHint _cdecl glfwWindowHint
#define glfwCreateWindow _cdecl glfwCreateWindow
#define glfwGetPrimaryMonitor _cdecl glfwGetPrimaryMonitor
#define glfwPollEvents _cdecl glfwPollEvents
#define glfwWaitEvents _cdecl glfwWaitEvents
#define glfwGetCursorPos _cdecl glfwGetCursorPos
#define glfwGetMouseButton _cdecl glfwGetMouseButton
#define glfwSetMouseButtonCallback _cdecl glfwSetMouseButtonCallback
#define glfwSetCursorPosCallback _cdecl glfwSetCursorPosCallback
#define glfwSetInputMode _cdecl glfwSetInputMode
#define glfwWindowShouldClose _cdecl glfwWindowShouldClose
#define glfwSetWindowFocusCallback _cdecl glfwSetWindowFocusCallback
#define glfwTerminate _cdecl glfwTerminate
#define glfwMakeContextCurrent _cdecl glfwMakeContextCurrent
#define glfwSwapBuffers _cdecl glfwSwapBuffers
#define glfwGetWin32Window _cdecl glfwGetWin32Window
#define glfwSetWindowMonitor _cdecl glfwSetWindowMonitor

#include <../3rdparty/glfw-3.3.2.bin.WIN32/include/GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <../3rdparty/glfw-3.3.2.bin.WIN32/include/GLFW/glfw3native.h>

#undef glfwInit
#undef glfwWindowHint
#undef glfwCreateWindow
#undef glfwGetPrimaryMonitor
#undef glfwPollEvents
#undef glfwWaitEvents
#undef glfwGetCursorPos
#undef glfwGetMouseButton
#undef glfwSetMouseButtonCallback
#undef glfwSetCursorPosCallback
#undef glfwSetInputMode
#undef glfwWindowShouldClose
#undef glfwSetWindowFocusCallback
#undef glfwTerminate
#undef glfwMakeContextCurrent
#undef glfwSwapBuffers
#undef glfwGetWin32Window
#undef glfwSetWindowMonitor
