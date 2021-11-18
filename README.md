<!--
SPDX-FileCopyrightText: 2021 Andrea Pappacoda

SPDX-License-Identifier: AGPL-3.0-or-later
-->

# cloudflare-ddns

[![Linux](https://github.com/Tachi107/cloudflare-ddns/actions/workflows/linux.yaml/badge.svg)](https://github.com/Tachi107/cloudflare-ddns/actions/workflows/linux.yaml)
[![Windows](https://ci.appveyor.com/api/projects/status/xe5wo63pxht8pd6n?svg=true)](https://ci.appveyor.com/project/Tachi107/cloudflare-ddns)
[![Language grade: C++](https://img.shields.io/lgtm/grade/cpp/g/Tachi107/cloudflare-ddns.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/Tachi107/cloudflare-ddns/context:cpp)
[![REUSE status](https://api.reuse.software/badge/github.com/Tachi107/cloudflare-ddns)](https://api.reuse.software/info/github.com/Tachi107/cloudflare-ddns)

cloudflare-ddns is a tool that can be used to dynamically update a DNS record using Cloudflare's API.

## Usage

This tool is a oneshot program: you run it, it updates the DNS record, and it terminates. To make it run periodically you could use a systemd timer or a cron job.

To run the tool you'll need an [API token](https://dash.cloudflare.com/profile/api-tokens) and the Zone ID of the DNS that you want to update; you can get the latter in the Overview panel of your zone.

Once you got the executable you can use it in two ways: you can pass the API Token, the Zone ID and the record name as command line arguments or you can use a TOML configuration file, located in `/etc/cloudflare-ddns/config.toml` when installing the software in `/usr` (e.g. when using the Debian package), and in `$install_prefix/etc/cloudflare-ddns/config.toml` otherwise (`/usr/local/etc` by default); [here's the template](config.toml). If you prefer, you can even use a configuration file in a custom location, using `--config file-path`.

You can download the latest release from the GitHub Releases page, or, if you prefer, you can [build](#Build) the program yourself.

## Library

cloudflare-ddns is also a library! In fact, the command line tool is fully based around it. It is fully tested with CI jobs, so you can be sure that it will always work as expected.

## Build

libcloudflare-ddns relies on [libcurl](https://curl.se) and [simdjson](https://simdjson.org), while the binary also depends on [tomlplusplus](https://marzer.github.io/tomlplusplus/).

On Debian and derivatives, you can install the packages `meson`, `pkg-config`, `cmake`, `libcurl4-openssl-dev` and a recent version of `libsimdjson-dev`. You should be fine with the packages available in Debian 10 (+backports) or Ubuntu Hirsute.

After having installed the dependencies, you can build the program with `meson setup build` and then `meson compile -C build`. If your Meson version is too old, you have to run `ninja -C build` instead of `meson compile`.

If your libcurl is older than version 7.64.1 (for example in Debian 10) you'll see a lot of connection logs related to DNS over HTTPS lookups; that's a [libcurl bug](https://github.com/curl/curl/issues/3660), and there's nothing I can do about it :/

## Systemd timer

Here's an example of a systemd service + timer that checks and eventually updates one DNS record

### `cloudflare-ddns.service`

```ini
[Unit]
Description=Simple utility to dynamically change a DNS record
After=network-online.target

[Service]
Type=oneshot
# Sleep to avoid connection issues when running the script at boot
ExecStartPre=/usr/bin/sleep 20
ExecStart=/usr/local/bin/cloudflare-ddns <api_key> <zone_id> <dns_record>
User=www-data
Group=www-data
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true

[Install]
WantedBy=default.target
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
