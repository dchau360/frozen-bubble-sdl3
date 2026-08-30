{
  lib,
  clangStdenv,
  cmake,
  clang,
  ninja,
  sdl3,
  sdl3-ttf,
  sdl3-image,
  sdl3-mixer,
  libpng,
  libjpeg,
  libtiff,
  libwebp,
  iniparser,
  glib,
  pkg-config,
  ...
}:
clangStdenv.mkDerivation {
  pname = "frozen-bubble-sdl3";
  version = "2.4.54";  # keep in step with project(... VERSION ...) in CMakeLists.txt (REL-004)

  src = lib.cleanSource ./.;

  nativeBuildInputs = [ cmake ];

  buildInputs = [
    clang
    ninja
    sdl3
    sdl3-ttf
    sdl3-image
    sdl3-mixer
    libpng
    libjpeg
    libtiff
    libwebp
    iniparser
    glib
    pkg-config
  ];

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
  ];

  buildPhase = ''
    cmake . -B build -DASSET_PATH="$out/share" $cmakeFlags
    cmake --build build
  '';

  installPhase = ''
    mkdir -p "$out/bin"
    cp -r share "$out/share"
    cp build/frozen-bubble-sdl3 "$out/bin"
  '';

  meta = with lib; {
    description = "SDL3 C++ Port of Frozen-Bubble 2";
    homepage = "https://github.com/dchau360/frozen-bubble-sdl3";
    license = licenses.gpl2;
  };
}
