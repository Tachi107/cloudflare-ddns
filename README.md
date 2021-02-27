# cloudflare-ddns

cloudflare-ddns is a tool that can be used to dynamically update a DNS record using Cloudflare's API. 

## Usage

This tool is a oneshot program: you run it, it updates the DNS record, and it terminates. To make it run periodically you could use it with a systemd timer or a cron job.

To run the tool you'll need an [API token](https://dash.cloudflare.com/profile/api-tokens) and the Zone ID of the DNS that you want to update; you can get the latter in the Overview panel of your zone.

## Dependencies

cloudflare-ddns currently relies on fairly new versions of libcurl and simdjson. To be able to run the utility you need to install `libcurl4` and `libsimdjson5` on your system.

## Build

To build cloudflare-ddns you need to install `meson`, `cmake`, `libcurl4-openssl-dev`, `nlohmann-json3-dev` and a recent version of `libsimdjson-dev`. You should be fine with the packages available in Debian 11 and Ubuntu Hirsute.

After having installed the dependencies, you can build the program with `meson setup build --buildtype=relese && meson compile -C build`

## Systemd timer

Here's an example of a systemd service + timer that checks and eventually updates one DNS record

### `cloudflare-ddns.service`

```ini
[Unit]
Description=Simple utility to dynamically change a DNS record
After=network-online.target

[Service]
Type=oneshot
ExecStart=/opt/cloudflare-ddns <api_key> <zone_id> <dns_record>
User=www-data
Group=www-data
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true

[Install]
WantedBy=multi-user.target
```

### `cloudflare-ddns.timer`

```ini
[Unit]
Description=Run cloudflare-ddns every hour

[Timer]
OnUnitActiveSec=1h

[Install]
WantedBy=timers.target
```
