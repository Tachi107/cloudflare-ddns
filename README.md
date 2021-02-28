# cloudflare-ddns

cloudflare-ddns is a tool that can be used to dynamically update a DNS record using Cloudflare's API.

## Usage

This tool is a oneshot program: you run it, it updates the DNS record, and it terminates. To make it run periodically you could use a systemd timer or a cron job.

To run the tool you'll need an [API token](https://dash.cloudflare.com/profile/api-tokens) and the Zone ID of the DNS that you want to update; you can get the latter in the Overview panel of your zone.

## Dependencies

cloudflare-ddns relies only on libcurl and simdjson. I recommend you to compile the program yourself, but I also provide a statically linked executable for every release. It is not 100% self-contained but it should work in most cases.

## Build

To build cloudflare-ddns you'll need to install `meson`, `pkg-config`, `cmake`, `libcurl4-openssl-dev` and `libsimdjson-dev`. You should be fine with the packages available in Debian 10 (+backports) or Ubuntu Focal.

After having installed the dependencies, you can build the program with `meson setup build --buildtype=relese` and then `meson compile -C build`. If your Meson version is too old, you have to run `ninja -C build` instead of `meson compile`.

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
