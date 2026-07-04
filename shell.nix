{ pkgs ? import <nixpkgs> { } }:

pkgs.mkShell {
  # Toolchain matched to CodinGame, which compiles C++ with g++ (GCC).
  packages = with pkgs; [
    gcc          # g++ / gcc — primary compiler, matches the judge
    gdb          # local debugging
    clang-tools  # clangd for editor LSP (compile with g++, lint/complete with clangd)
  ];

  shellHook = ''
    echo "Mad Pod Racing dev shell — $(g++ --version | head -n1)"
    echo "Build: g++ -std=c++17 -O2 -Wall -Wextra -o pod main.cpp"
    echo "Run:   ./pod < tests/sample_input.txt"
  '';
}
