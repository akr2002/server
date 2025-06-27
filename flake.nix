{
  description = "C++ dev environment";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    nixvim.url = "github:nix-community/nixvim";
  };

  outputs = { self, nixpkgs, nixvim, ... }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux.pkgs;
      config = import ./.config;
      nixvimlib = nixvim.lib.x86_64-linux;
      vv = nixvim.legacyPackages.x86_64-linux.makeNixvimWithModule {
        inherit pkgs;
        module = config;
      };
      buildInputs =
        (with pkgs; [ gnumake clang_20 clang-tools clang-analyzer lldb_20 ])
        ++ (with pkgs.llvmPackages_20; [
          stdenv
          libcxxStdenv
          libcxxClang
          compiler-rt
          compiler-rt-libc
          clangUseLLVM
          libcxxClang
          libcxx
          libllvm
          lld
          bintools
        ]);
      CPATH = builtins.concatStringsSep ":" [
        (pkgs.lib.makeSearchPathOutput "dev" "include"
          [ pkgs.llvmPackages_20.libcxx ])
        (pkgs.lib.makeSearchPath "resource-root/include"
          [ pkgs.llvmPackages_20.clang ])
      ];
    in {
      packages.x86_64-linux.default =
        pkgs.llvmPackages_20.libcxxStdenv.mkDerivation {
          pname = "server";
          version = "0.1.0";
          src = ./.;
          inherit buildInputs CPATH;
          buildPhase = ''
            make
          '';
          installPhase = ''
            mkdir -p $out/bin
            cp build/server $out/bin/

            cp src/config.ini $out/bin/
          '';
        };
      devShell.x86_64-linux = pkgs.llvmPackages_20.libcxxStdenv.mkDerivation {
        name = "C++ dev environment";
        buildInputs = [ buildInputs vv ];
        inherit CPATH;
        shellHook = ''
          alias g=git
          alias ga='git add'
          alias gb='git branch'
          alias gc='git commit'
          alias gp='git push'
          alias gst='git status'
          alias vv='nvim'
        '';
      };
    };
}
