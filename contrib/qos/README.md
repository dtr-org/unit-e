### QoS (Quality of service) ###

This is a Linux bash script that will set up tc to limit the outgoing bandwidth for connections to the UnitE network. It limits outbound TCP traffic with a source or destination port of 7182, but not if the destination IP is within a LAN.

This means one can have an always-on united instance running, and another local united/unite-qt instance which connects to this node and receives blocks from it.
