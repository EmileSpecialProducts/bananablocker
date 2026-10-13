# 🍌 BananaBlocker



A Pi-hole-like ad blocker that runs on ESP8266 microcontrollers. Block ads and trackers at the DNS level for your entire network!

Attention: Code Usage Terms

You are free to use this code for personal and educational purposes, and you may fork the project. However, you must credit me by mentioning my name and including a link to the original project whenever you modify or redistribute the code.

This project is intended for educational purposes only. I reserve the right to take legal action against anyone who uses this code to create or publish content without proper authorization.

## Features

- **DNS-based Ad Blocking**: Intercepts DNS requests and blocks known ad servers
- **Web-based Admin Panel**: Beautiful, responsive interface for management
- **Initial Setup Wizard**: Captive portal for easy WiFi configuration
- **Statistics Dashboard**: Track blocked vs allowed requests
- **Custom Block Lists**: Add/remove domains via web interface
- **Factory Reset**: Hardware button reset (4-second press)
- **Persistent Storage**: Settings saved to SPIFFS (flash file system)

## Hardware Requirements

- **ESP ESP12e, ESP-32 (S2/S3/C2/C3/C5/C6/C61)** 
- **Power Supply**: 3.3V or USB power
- **Flash Button**: Built-in flash button for factory reset

## Quick Start

### 1. Upload the Code

1. Upload the software using the [web instaler](https://emilespecialproducts.github.io/bananablocker/upload.html)  

### 2. Initial Setup

1. **First Boot**: Device creates "BananaBlocker" WiFi hotspot
2. **Connect**: Join the hotspot (password: `banana123`)
3. **Configure**: Browser automatically opens setup page
4. **WiFi Setup**: Select your network and enter credentials
5. **Admin Password**: Set admin password
6. **Complete**: Device restarts and connects to your network

### 3. Access Admin Panel

1. **Find IP**: Check your router or use network scanner
2. **Login**: Navigate to device IP in browser
3. **Dashboard**: View statistics and manage block lists

All required libraries are built into the Arduino Core.

## Default Configuration

- **AP SSID**: `BananaBlocker`
- **AP Password**: `banana123`
- **Web Port**: `80`
- **DNS Port**: `53`


### Admin Dashboard

Access via device IP address after setup:

**Login Page:**
- Shows current statistics (blocked/allowed requests)
- Password-protected access

**Dashboard:**
- Real-time statistics
- Domain management interface
- System settings

**Block List Management:**
- Add domains to block
- Remove existing domains
- View all blocked domains

### DNS Configuration

To use BananaBlocker as your DNS server:

**Option 1: Router Configuration**
- Set router's primary DNS to BananaBlocker IP
- All devices will use BananaBlocker automatically


## Factory Reset

**Software Reset:**
- Access admin panel → System Settings → Factory Reset

**Hardware Reset:**
- Press and hold **Flash button** for **4+ seconds**
- Device will restart in setup mode

## Default Blocked Domains

BananaBlocker comes pre-configured with common ad servers:

- `googleads.g.doubleclick.net`
- `googlesyndication.com`
- `google-analytics.com`
- `googletagmanager.com`
- `facebook.com/tr`
- `ads.yahoo.com`
- `advertising.com`
- `adsystem.amazon.com`
- `amazon-adsystem.com`
- `outbrain.com`
- `taboola.com`
- `scorecardresearch.com`
- `adsafeprotected.com`
- `moatads.com`

## Technical Details

### Memory Usage
- **EEPROM**: 4KB for configuration storage
- **SPIFFS**: File system for web assets
- **RAM**: ~50KB free during operation

### Network Configuration
- **Station Mode**: Connects to existing WiFi
- **AP Mode**: Creates captive portal for setup
- **mDNS**: Accessible via `bananablock.local`



## Troubleshooting

### Can't Connect to Setup Network
- Ensure device is in AP mode (first boot or after reset)
- Network name should be "BananaBlocker"
- Try forgetting and reconnecting to the network

### Setup Page Won't Load
- Navigate directly to `192.168.4.1`
- Clear browser cache
- Try different browser

### WiFi Connection Fails
- Check SSID and password spelling
- Ensure 2.4GHz network (ESP8266 doesn't support 5GHz)
- Move device closer to router



### DNS Not Working
- Verify DNS settings on router/devices
- Check that device IP is correct
- Restart router and devices

### Factory Reset Not Working
- Hold flash button for full 4+ seconds
- Button is usually marked "FLASH" or "BOOT"
- Try software reset via admin panel

## API Endpoints

For developers and advanced users:

```
GET  /                    - Main page (login or dashboard)
GET  /setup              - Setup page (AP mode only)
POST /setup              - Save WiFi configuration
POST /login              - Authenticate user
GET  /logout             - Logout user
GET  /api/stats          - Get statistics (JSON)
GET  /api/blocklist      - Get blocked domains (JSON)
POST /api/blocklist      - Add domain to blocklist
DELETE /api/blocklist    - Remove domain from blocklist
POST /api/reset          - Factory reset
```

## Development

### Customization

**Change Default Passwords:**
```cpp
const char* ap_password = "your_password";
strcpy(config.admin_password, "your_admin_password");
```

**Add More Default Domains:**
```cpp
const char* default_blocked_domains[] = {
    "your.domain.com",
    // ... existing domains
};
```

**Modify UI Colors:**
Edit the CSS in the HTML page functions.

### Building from Source

1. Clone or download the project
2. Open `BananaBlocker.ino` in Arduino IDE
3. Verify board settings:
   - Board: Your ESP8266 variant
   - CPU Frequency: 80 MHz
   - Flash Size: 4MB (FS: 2MB, OTA: ~1019KB)
4. Upload to device

## License

Attention: Code Usage Terms

You are free to use this code for personal and educational purposes, and you may fork the project. However, you must credit me by mentioning my name and including a link to the original project whenever you modify or redistribute the code.

This project is intended for educational purposes only. I reserve the right to take legal action against anyone who uses this code to create or publish content without proper authorization.

## Support

For issues and questions:
1. Check troubleshooting section
2. Verify hardware connections
3. Check serial monitor output
4. Reset to factory defaults

---

**Made with 🍌 for a better, ad-free internet!**

