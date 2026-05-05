NetPerfMeter QUIC Setup
=======================

#

TBD ...


# Wireshark NetPerfMeter/QUIC Traffic Sniffing

1. Prepare a location for the TLS key log:
```bash
mkdir -m700 -p /home/$USER/keylog
touch /home/$USER/keylog/sslkeylog.log
chmod 700 /home/$USER/keylog/sslkeylog.log
```

2. Start programs with `SSLKEYLOGFILE` environment variable, to let them
write the key log:
```bash
SSLKEYLOGFILE=/home/$USER/keylog/sslkeylog.log netperfmeter ...
```

3. Configure Wireshark:

* Preferences
  * Protocols
    * TLS
      - Specify Key Log File:
        Select your `/home/$USER/keylog/sslkeylog.log` file here!
