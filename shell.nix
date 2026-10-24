{pkgs ? import <nixpkgs> {}}: let
  crossPkgs = pkgs.pkgsCross.i686-embedded;
in
  pkgs.mkShell {
    nativeBuildInputs = [
      crossPkgs.buildPackages.gcc
      crossPkgs.buildPackages.binutils
      pkgs.gdb
      pkgs.cgdb
      pkgs.qemu
      pkgs.bochs
      pkgs.perl
      pkgs.gnumake
    ];

    shellHook = ''
      # Isolate Nix-specific setup to avoid dirtying the Git tree
      ENV_DIR=".nix-env"
      mkdir -p "$ENV_DIR/utils" "$ENV_DIR/bin"

      # 1. Non-destructively patch and compile Pintos utilities
      if [ -d "src/utils" ]; then
        cp -r src/utils/* "$ENV_DIR/utils/"
        chmod -R +w "$ENV_DIR/utils" # Ensure we can modify the copied files

        # Use Nix's native function to dynamically fix /usr/bin/perl shebangs
        patchShebangs "$ENV_DIR/utils" >/dev/null 2>&1

        # Build the C utilities (squish-pty, squish-unix) inside the hidden dir
        make -C "$ENV_DIR/utils" clean >/dev/null 2>&1 || true
        make -C "$ENV_DIR/utils" >/dev/null 2>&1 || true
      fi

      # 2. Compatibility wrappers for modern GCC/binutils with legacy Pintos
      REAL_GCC=$(which gcc)
      cat << EOF > "$ENV_DIR/bin/gcc"
#!/bin/sh
exec "$REAL_GCC" -B$PWD/$ENV_DIR/bin -fcommon "\$@"
EOF
      chmod +x "$ENV_DIR/bin/gcc"

      REAL_LD=$(which ld)
      cat << EOF > "$ENV_DIR/bin/ld"
#!/bin/sh
exec "$REAL_LD" -z noseparate-code "\$@"
EOF
      chmod +x "$ENV_DIR/bin/ld"

      # Also alias i386-elf-* binaries to match prefixes Pintos expects
      ln -sf "$ENV_DIR/bin/gcc" "$ENV_DIR/bin/i386-elf-gcc"
      ln -sf "$ENV_DIR/bin/ld" "$ENV_DIR/bin/i386-elf-ld"
      ln -sf $(which i686-elf-as) "$ENV_DIR/bin/i386-elf-as" 2>/dev/null || true
      ln -sf $(which i686-elf-objdump) "$ENV_DIR/bin/i386-elf-objdump" 2>/dev/null || true
      ln -sf $(which i686-elf-objcopy) "$ENV_DIR/bin/i386-elf-objcopy" 2>/dev/null || true
      ln -sf $(which i686-elf-addr2line) "$ENV_DIR/bin/i386-elf-addr2line" 2>/dev/null || true
      ln -sf $(which gdb) "$ENV_DIR/bin/i386-elf-gdb" 2>/dev/null || true

      # 3. Prepend our isolated utilities and aliases to PATH
      export PATH="$PWD/$ENV_DIR/utils:$PWD/$ENV_DIR/bin:$PATH"

      # Ensure the pintos script can find Pintos.pm in the new directory
      export PERL5LIB="$PWD/$ENV_DIR/utils:$PERL5LIB"

      # 4. Hide the Nix environment directory from Git status silently
      if [ -d ".git" ] && ! grep -q "^.nix-env$" .git/info/exclude 2>/dev/null; then
        echo ".nix-env" >> .git/info/exclude
      fi

      echo "Pintos Nix environment ready!"
    '';
  }
