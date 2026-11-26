# Prepare a y21ga microSD on Windows

This optional tool is for cameras with the confirmed `y21ga` hardware
suffix. It uses the existing microSD Wi-Fi setup in the official 0.3.7
release. It verifies the release archive checksum, enables Disable Cloud,
and adds a boot report that can be read from the card when the camera is
unreachable. Check the supported camera table before selecting a package.

1. Keep a copy of your current card. Use an empty FAT32 microSD for the new
   setup. The tool never formats a drive or replaces an existing setup.
2. Open PowerShell in this directory and run the command below. Replace
   `E` with the drive letter of the new card.

   ```powershell
   .\prepare-y21ga.ps1 E:/
   ```

   Enter the 2.4 GHz network name and WPA2 password when prompted. Add
   `-KeepWifi` if the camera already has the correct Wi-Fi credentials, or
   `-Archive .\y21ga_0.3.7.tgz` to use a previously downloaded release.
3. Power off the camera, insert the prepared card, and power it on. Leave
   the card in the camera. The installation can reboot once while applying
   Wi-Fi credentials.

## Troubleshooting

If the camera does not appear on the local network, power it off and
check `YI-LOCAL-STATUS.txt` in the card root.

| Status | Meaning |
| --- | --- |
| `waiting_for_first_boot` | The boot report has not run. Check the card layout and the upstream `hack_result.txt` file. |
| `startup_hook_reached` | The boot report started. The Wi-Fi interface may still be waiting for an address. |
| `wifi_ipv4_assigned` | The Wi-Fi interface had an IPv4 address. Look at the `ip` field. |
| `no_wifi_ipv4_after_60s` | The Wi-Fi interface had no IPv4 address after one minute. Check the network name, credentials, and access point. |

An IP address confirms local addressing. Verify the web interface and RTSP
stream separately. The report records configuration flags, never the Wi-Fi
password. The installer still places the supplied password in
`Factory/configure_wifi.cfg`, which normally ends up under `Factory.done`.
You may remove that file after confirming Wi-Fi works across restarts.
Do not publish the card contents or camera flash backups.

The tool prepares the released 0.3.7 web interface as shipped. Configuration
save verification in the current source applies to firmware built from
that source.
