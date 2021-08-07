with import <nixos> {};

stdenv.mkDerivation {
  pname="bcache";
  version="0.1.0";
  src=./.;
  buildInputs = [ makeWrapper nixUnstable.dev boost.dev nlohmann_json ];
  installPhase=''
    mkdir -pv $out/bin
    cp bcache $out/bin
    wrapProgram $out/bin/bcache \
      --prefix PATH ":" ${lib.makeBinPath [ nix ]} 
  '';
}
