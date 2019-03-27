Sample init scripts and service configuration for unit-e
==========================================================

Sample scripts and configuration files for systemd, Upstart and OpenRC
can be found in the contrib/init folder.

    contrib/init/unit-e.service:    systemd service unit configuration
    contrib/init/unit-e.openrc:     OpenRC compatible SysV style init script
    contrib/init/unit-e.openrcconf: OpenRC conf.d file
    contrib/init/unit-e.conf:       Upstart service configuration file
    contrib/init/unit-e.init:       CentOS compatible SysV style init script

Service User
---------------------------------

All three Linux startup configurations assume the existence of a "unite" user
and group.  They must be created before attempting to use these scripts.
The OS X configuration assumes unit-e will be set up for the current user.

Configuration
---------------------------------

At a bare minimum, unit-e requires that the rpcpassword setting be set
when running as a daemon.  If the configuration file does not exist or this
setting is not set, unit-e will shutdown promptly after startup.

This password does not have to be remembered or typed as it is mostly used
as a fixed token that unit-e and client programs read from the configuration
file, however it is recommended that a strong and secure password be used
as this password is security critical to securing the wallet should the
wallet be enabled.

If unit-e is run with the "-server" flag (set by default), and no rpcpassword is set,
it will use a special cookie file for authentication. The cookie is generated with random
content when the daemon starts, and deleted when it exits. Read access to this file
controls who can access it through RPC.

By default the cookie is stored in the data directory, but it's location can be overridden
with the option '-rpccookiefile'.

This allows for running unit-e without having to do any manual configuration.

`conf`, `pid`, and `wallet` accept relative paths which are interpreted as
relative to the data directory. `wallet` *only* supports relative paths.

Paths
---------------------------------

### Linux

All three configurations assume several paths that might need to be adjusted.

Binary:              `/usr/bin/unit-e`
Configuration file:  `/etc/unite/unit-e.conf`
Data directory:      `/var/lib/unit-e`
PID file:            `/var/run/unit-e/unit-e.pid` (OpenRC and Upstart) or `/var/lib/unit-e/unit-e.pid` (systemd)
Lock file:           `/var/lock/subsys/unit-e` (CentOS)

The configuration file, PID directory (if applicable) and data directory
should all be owned by the unite user and group.  It is advised for security
reasons to make the configuration file and data directory only readable by the
unite user and group.  Access to unit-e-cli and other unit-e rpc clients
can then be controlled by group membership.

### Mac OS X

Binary:              `/usr/local/bin/unit-e`
Configuration file:  `~/Library/Application Support/Unit-e/unit-e.conf`
Data directory:      `~/Library/Application Support/Unit-e`
Lock file:           `~/Library/Application Support/Unit-e/.lock`

Installing Service Configuration
-----------------------------------

### systemd

Installing this .service file consists of just copying it to
/usr/lib/systemd/system directory, followed by the command
`systemctl daemon-reload` in order to update running systemd configuration.

To test, run `systemctl start unit-e` and to enable for system startup run
`systemctl enable unit-e`

### OpenRC

Rename unit-e.openrc to unit-e and drop it in /etc/init.d.  Double
check ownership and permissions and make it executable.  Test it with
`/etc/init.d/unit-e start` and configure it to run on startup with
`rc-update add unit-e`

### Upstart (for Debian/Ubuntu based distributions)

Drop unit-e.conf in /etc/init.  Test by running `service unit-e start`
it will automatically start on reboot.

NOTE: This script is incompatible with CentOS 5 and Amazon Linux 2014 as they
use old versions of Upstart and do not supply the start-stop-daemon utility.

### CentOS

Copy unit-e.init to /etc/init.d/unit-e. Test by running `service unit-e start`.

Using this script, you can adjust the path and flags to the unit-e program by
setting the UNIT_E and FLAGS environment variables in the file
/etc/sysconfig/unit-e. You can also use the DAEMONOPTS environment variable here.

### Mac OS X

Copy org.unite.unit-e.plist into ~/Library/LaunchAgents. Load the launch agent by
running `launchctl load ~/Library/LaunchAgents/org.unite.unit-e.plist`.

This Launch Agent will cause unit-e to start whenever the user logs in.

NOTE: This approach is intended for those wanting to run unit-e as the current user.
You will need to modify org.unite.unit-e.plist if you intend to use it as a
Launch Daemon with a dedicated unite user.

Auto-respawn
-----------------------------------

Auto respawning is currently only configured for Upstart and systemd.
Reasonable defaults have been chosen but YMMV.
