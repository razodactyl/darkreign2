#pragma once

#define glfwInit _cdecl glfwInit
#define glfwWindowHint _cdecl glfwWindowHint
#define glfwCreateWindow _cdecl glfwCreateWindow
#define glfwWindowShouldClose _cdecl glfwWindowShouldClose
#define glfwTerminate _cdecl glfwTerminate
#define glfwMakeContextCurrent _cdecl glfwMakeContextCurrent
#define glfwSwapBuffers _cdecl glfwSwapBuffers

#include <GLFW/glfw3.h>

#undef glfwInit
#undef glfwWindowHint
#undef glfwCreateWindow
#undef glfwWindowShouldClose
#undef glfwTerminate
#undef glfwMakeContextCurrent
#undef glfwSwapBuffers
