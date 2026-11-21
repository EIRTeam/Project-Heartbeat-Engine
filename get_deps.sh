set -e

mkdir -p buildeps
cd buildeps

# Linux ffmpeg
wget https://github.com/EIRTeam/FFmpeg-Builds/releases/download/godot-9.0/ffmpeg-master-latest-linux64-lgpl-godot.tar.xz
tar xf ffmpeg-master-latest-linux64-lgpl-godot.tar.xz

# Windows ffmpeg
wget https://github.com/EIRTeam/FFmpeg-Builds/releases/download/godot-9.0/ffmpeg-master-latest-win64-lgpl-godot.tar.xz
tar xf ffmpeg-master-latest-win64-lgpl-godot.tar.xz

# SDL for windows
wget https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-mingw.tar.gz
tar xf SDL2-devel-2.30.9-mingw.tar.gz
mv "SDL2-2.30.9" "sdl_mingw"
