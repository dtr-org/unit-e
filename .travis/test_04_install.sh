

if [ -n "$DPKG_ADD_ARCH" ]; then
  sudo dpkg --add-architecture "$DPKG_ADD_ARCH" 
fi

if [ -n "$PACKAGES" ]; then
  travis_retry sudo apt-get update
fi

if [ -n "$PACKAGES" ]; then
  travis_retry sudo apt-get install --no-install-recommends --no-upgrade -qq $PACKAGES
fi

