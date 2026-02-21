{ lib, stdenv, cmake, autoPatchelfHook }:

let
  ortArch = if stdenv.hostPlatform.isAarch64 then "aarch64" else "x86_64";
in
stdenv.mkDerivation {
  pname = "moonshine";
  version = "0.0.48";

  src = ./..;

  nativeBuildInputs = [ cmake autoPatchelfHook ];
  buildInputs = [ stdenv.cc.cc.lib ];

  postPatch = ''
    sed -i 's|''${CMAKE_CURRENT_LIST_DIR}/ort-utils/build|''${CMAKE_CURRENT_BINARY_DIR}/ort-utils|' core/CMakeLists.txt
    sed -i 's|''${CMAKE_CURRENT_LIST_DIR}/bin-tokenizer/build|''${CMAKE_CURRENT_BINARY_DIR}/bin-tokenizer|' core/CMakeLists.txt
    sed -i 's|''${CMAKE_CURRENT_LIST_DIR}/../third-party/onnxruntime/build|''${CMAKE_CURRENT_BINARY_DIR}/onnxruntime|' core/ort-utils/CMakeLists.txt
    sed -i 's|''${CMAKE_CURRENT_LIST_DIR}/../moonshine-utils/build|''${CMAKE_CURRENT_BINARY_DIR}/moonshine-utils|' core/ort-utils/CMakeLists.txt
    sed -i 's/-Werror/-Werror -Wno-error=unused-result/' core/CMakeLists.txt
  '';

  cmakeDir = "../core";

  buildPhase = ''
    runHook preBuild
    cmake --build . --target moonshine -j$NIX_BUILD_CORES
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/lib $out/include $out/share/moonshine/models

    install -m755 libmoonshine.so $out/lib/
    patchelf --set-rpath $out/lib $out/lib/libmoonshine.so
    install -m755 ../core/third-party/onnxruntime/lib/linux/${ortArch}/libonnxruntime.so.1 $out/lib/

    install -m644 ../core/moonshine-c-api.h $out/include/
    install -m644 ../core/moonshine-cpp.h $out/include/

    cp -r ../test-assets/tiny-en $out/share/moonshine/models/
    cp -r ../examples/android/Transcriber/app/src/main/assets/base-en $out/share/moonshine/models/

    runHook postInstall
  '';

  meta = with lib; {
    description = "Moonshine speech-to-text library";
    license = licenses.mit;
    platforms = [ "x86_64-linux" "aarch64-linux" ];
  };
}
