# config.py


def configure(env):
    pass
    # env.Prepend(CFLAGS=["-std=c++17"])
    # env.Prepend(CXXFLAGS=["-std=gnu++14"])


def can_build(env, platform):
    env.module_add_dependencies("shinobu", ["vorbis"])
    return True


def configure(env):
    pass
