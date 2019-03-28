package=hidapi
$(package)_version=0.9.0-rc1
$(package)_download_path=https://github.com/particl/hidapi/archive/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=a91aef254a5374e2c26860d0c3820613c2c8c3cdbbfdce417a1171e91db35797
$(package)_linux_dependencies=libusb

define $(package)_set_vars
  $(package)_config_opts=--disable-shared
  $(package)_config_opts_linux=--with-pic
endef

define $(package)_preprocess_cmds
  sed -i '64,67d' configure.ac; \
  sed -i '64iLIBS_HIDRAW_PR+=" -ludev"' configure.ac; \
  cd $($(package)_build_subdir); ./bootstrap
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
