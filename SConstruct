env = Environment()
src = Glob('#src/*.cpp')

env.Append(CPPPATH=['#include', '/usr/include/apr-1.0'])
env.Append(CFLAGS=['-O0', '-g'])

objs = []
for s in src:
    o = env.SharedObject(s)
    objs.append(o)

env.SharedLibrary(target='#lib/tglib', source = objs)
