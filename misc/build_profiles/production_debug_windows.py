import os

platform = "windows"
production = "yes"
sdl = "yes"
sdl_path = os.path.abspath("../buildeps/sdl_msvc")
target = "template_debug"
ffmpeg_path = os.path.abspath("../buildeps/ffmpeg-master-latest-win64-lgpl-godot")
lto = "none"
use_breakpad = "yes"
debug_symbols = "yes"
separate_debug_symbols = "yes"
custom_modules = "../modules"
d3d12 = "no"