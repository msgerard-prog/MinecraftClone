# Sourced: sets CLANG_FORMAT to Visual Studio's bundled clang-format.exe.
VSDIR="$('/mnt/c/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' -latest -products '*' \
  -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | tr -d '\r')"
CLANG_FORMAT="$(wslpath -u "$VSDIR")/VC/Tools/Llvm/x64/bin/clang-format.exe"
