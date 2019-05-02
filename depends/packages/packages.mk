packages:=boost openssl libevent zeromq

wallet_packages=bdb

upnp_packages=miniupnpc
usb_packages=hidapi

ifneq (,$(findstring linux,$(host)))
usb_packages += libusb
endif

darwin_native_packages = native_biplist native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus
endif
