{ lib, stdenv, moonshine }:

stdenv.mkDerivation {
  pname = "moonshine-cli";
  version = moonshine.version;

  src = ./moonshine-cli.cpp;
  dontUnpack = true;

  buildPhase = ''
    $CXX -std=c++20 -O2 -Wall \
      -I${moonshine}/include \
      -L${moonshine}/lib \
      -o moonshine-cli $src \
      -lmoonshine \
      -Wl,-rpath,${moonshine}/lib
  '';

  installPhase = ''
    runHook preInstall
    mkdir -p $out/bin
    install -m755 moonshine-cli $out/bin/
    runHook postInstall
  '';

  meta = with lib; {
    description = "Moonshine speech-to-text CLI (whisper-cli compatible)";
    license = licenses.mit;
    platforms = moonshine.meta.platforms;
  };
}
