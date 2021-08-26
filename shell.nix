# worked for sure with commit d4590d21006
with import <nixos> {};

mkShell {
  buildInputs = [ gcc nixUnstable.dev boost.dev nlohmann_json zstd ];
}
