set -e

mkdir -p buildeps
cd buildeps
rm -rf retail_bin
mkdir -p retail_bin
cd retail_bin

wget -N https://github.com/EIRTeam/Project-Heartbeat-Engine/releases/download/latest/windows-engine-debug.zip
unzip -o windows-engine-debug
wget -N https://github.com/EIRTeam/Project-Heartbeat-Engine/releases/download/latest/windows-editor.zip
unzip -o windows-editor
wget -N https://github.com/EIRTeam/Project-Heartbeat-Engine/releases/download/latest/windows-engine.zip
unzip -o windows-engine
wget -N https://github.com/EIRTeam/Project-Heartbeat-Engine/releases/download/latest/linux-engine.zip
unzip -o linux-engine
wget -N https://github.com/EIRTeam/Project-Heartbeat-Engine/releases/download/latest/android-engine.zip
unzip -o android-engine
wget -N https://github.com/EIRTeam/Project-Heartbeat-Engine/releases/download/latest/linux-editor.zip
unzip -o linux-editor
wget -N https://github.com/EIRTeam/Project-Heartbeat-Engine/releases/download/latest/godot.linuxbsd.template_release.x86_64.syms
chmod +x *.x86_64