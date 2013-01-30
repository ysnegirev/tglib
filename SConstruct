import sys
env = Environment()
src = Glob('#src/*.cpp')

env.Append(CPPPATH=['#include'])
env.Append(CFLAGS=['-O0', '-g'])
env.Append(CXXFLAGS=['-O0', '-g'])
env.Append(LIBS='apr-1')
if 'linux' in sys.platform:
    env.Append(CPPPATH=['/usr/include/apr-1.0'])
elif 'darwin' in sys.platform:
    env.Append(CPPPATH='/opt/local/include/apr-1')


objs = []
for s in src:
    o = env.SharedObject(s)
    objs.append(o)

env.SharedLibrary(target='#lib/tglib', source = objs)
