{ pkgs }:

pkgs.mkShell {
  packages = with pkgs; [
    cmake
    gcc
    gdb
    patchelf
  ];

  shellHook = ''
    export LD_LIBRARY_PATH="$PWD/core/third-party/onnxruntime/lib/linux/$(uname -m)''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
  '';
}
