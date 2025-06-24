{ lib, config, ... }: {
  imports = [
    ./colorizer.nix
    ./mini.nix
    ./neotest.nix
    ./nvim-autopairs.nix
    ./nvim-surround.nix
    ./nvterm.nix
    ./oil.nix
    ./plenary.nix
    ./project-nvim.nix
    ./sidebar.nix
    ./todo-comments.nix
    ./ultimate-autopair.nix
    ./undotree.nix
    ./which-key.nix
    ./wilder.nix
  ];

  options = { utils.enable = lib.mkEnableOption "Enable utils module"; };
  config = lib.mkIf config.utils.enable {
    mini.enable = lib.mkDefault true;
    neotest.enable = lib.mkDefault false;
    nvim-autopairs.enable = lib.mkDefault true;
    colorizer.enable = lib.mkDefault true;
    nvim-surround.enable = lib.mkDefault true;
    nvterm.enable = lib.mkDefault true;
    oil.enable = lib.mkDefault true;
    plenary.enable = lib.mkDefault true;
    project-nvim.enable = lib.mkDefault true;
    sidebar.enable = lib.mkDefault true;
    todo-comments.enable = lib.mkDefault true;
    ultimate-autopair.enable = lib.mkDefault true;
    undotree.enable = lib.mkDefault true;
    which-key.enable = lib.mkDefault true;
    wilder.enable = lib.mkDefault false;
  };
}
