import os

platform = "windows"
production = "yes"
sdl = "yes"
sdl_path = os.path.abspath("../buildeps/sdl_msvc")
target = "editor"
ffmpeg_path = os.path.abspath("../buildeps/ffmpeg-master-latest-win64-lgpl-godot")
lto = "none"
use_breakpad = "yes"
debug_symbols = "yes"
separate_debug_symbols = "yes"
custom_modules = "../modules"
scu_build = "yes"
d3d12 = "no"