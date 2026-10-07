# glad 2 loader, generated for OpenGL 4.6 core + GL_KHR_debug:
#   python -m glad --api gl:core=4.6 --extensions=GL_KHR_debug --out-path glad c
add_library(glad STATIC glad/src/gl.c)
target_include_directories(glad SYSTEM PUBLIC glad/include)
