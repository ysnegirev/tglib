import sys
env = Environment()

env.Append(CPPPATH=['#include'])
env.Append(CFLAGS=['-O0', '-g'])
env.Append(CXXFLAGS=['-O0', '-g'])
env.Append(CPPDEFINES='OS_UNIX')
if 'linux' in sys.platform:
    env.Append(CPPPATH=['/usr/include/apr-1.0'])
elif 'darwin' in sys.platform:
    env.Append(CPPPATH='/opt/local/include/apr-1')

Export('env')

f = env.SConscript('#test/single/SConscript')
