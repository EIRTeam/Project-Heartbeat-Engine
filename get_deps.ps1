choco install wget -y
choco install 7zip -y # Needed for tar... smh

if (Test-Path -Path buildeps) {
    Remove-Item buildeps -Recurse -Force
}
New-Item -ItemType Directory buildeps

# Windows ffmpeg
Write-Output "Downloading ffmpeg"
C:\ProgramData\chocolatey\lib\Wget\tools\wget.exe https://github.com/EIRTeam/FFmpeg-Builds/releases/download/godot-9.0/ffmpeg-master-latest-win64-lgpl-godot.tar.xz -O "buildeps/ffmpeg.tar.xz"

7z x -obuildeps buildeps/ffmpeg.tar.xz
7z x -obuildeps buildeps/ffmpeg.tar

# SDL for windows
Write-Output "Downloading SDL"
C:\ProgramData\chocolatey\lib\Wget\tools\wget.exe https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-devel-2.30.9-VC.zip -O "buildeps/SDL.zip"
Expand-Archive -Path buildeps/SDL.zip -DestinationPath "buildeps/sdl_msvc"
mv buildeps/sdl_msvc/SDL2-2.30.9/* buildeps/sdl_msvc
