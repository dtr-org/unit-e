

unset CC
unset CXX

mkdir -p depends/SDKs depends/sdk-sources

if [ -n "$OSX_SDK" -a ! -f depends/sdk-sources/MacOSX${OSX_SDK}.sdk.tar.gz ]; then
  curl --location --fail $SDK_URL/MacOSX${OSX_SDK}.sdk.tar.gz -o depends/sdk-sources/MacOSX${OSX_SDK}.sdk.tar.gz
fi

if [ -n "$OSX_SDK" -a -f depends/sdk-sources/MacOSX${OSX_SDK}.sdk.tar.gz ]; then
  tar -C depends/SDKs -xf depends/sdk-sources/MacOSX${OSX_SDK}.sdk.tar.gz
fi

if [ -z "$NO_DEPENDS" ]; then
  make $MAKEJOBS -C depends HOST=$HOST $DEP_OPTS
fi

# Start xvfb if needed, as documented at https://docs.travis-ci.com/user/gui-and-headless-browsers/#Using-xvfb-to-Run-Tests-That-Require-a-GUI
if [ "$NEED_XVFB" = 1 ]; then
  export DISPLAY=:99.0; /sbin/start-stop-daemon --start --pidfile /tmp/custom_xvfb_99.pid --make-pidfile --background --exec /usr/bin/Xvfb -- :99 -ac
fi

