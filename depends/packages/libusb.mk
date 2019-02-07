package=libusb
$(package)_version=1.0.22
$(package)_download_path=https://github.com/libusb/libusb/archive/
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=3500f7b182750cd9ccf9be8b1df998f83df56a39ab264976bdb3307773e16f48

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --enable-udev
  $(package)_config_opts_linux=--with-pic
endef

define $(package)_preprocess_cmds
  cd $($(package)_build_subdir); ./bootstrap.sh
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
