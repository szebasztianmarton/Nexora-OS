# 100 gyakran használt játékkontroller Linux driverrel

> **Megjegyzés:** Nincs hivatalos, globális „top 100 leggyakoribb gamepad” rangsor. Ez a lista a Linuxon leggyakrabban használt, széles körben elterjedt kontrollereket tartalmazza. A legtöbb modern gamepad HID-eszköz, ezért sok modell a beépített `hid-generic` driverrel is működik.

## Támogatás a Nexora OS-ben

A Nexora image kernele ([`linux.fragment`](../buildroot-external/board/nexora-x86_64/linux.fragment)) a lista összes kernelben lévő USB-s driverét modulként tartalmazza. Bedugáskor az eudev a modalias alapján betölti a megfelelő modult, a konzol UI pedig a `/dev/input` figyelésével kb. egy másodpercen belül felismeri a kontrollert, és kihúzáskor el is engedi ([controller-support.md](controller-support.md#evdev-default-on-linux)). A gombok helyét driverenként javítja (pl. az `xpad` X/Y cseréjét).

| Csoport (sorszám) | USB-s bedugás után | Megjegyzés |
|---|---|---|
| Xbox-kompatibilis, `xpad` (1–12, 14–20) | működik | gombkiosztás: `xbox-labels`; az Xbox One/Series padok Bluetooth-on nem |
| Xbox Wireless Adapter, `xone` (13) | nem | kernelen kívüli driver, nincs az image-ben |
| PlayStation, `hid-sony` / `hid-playstation` (21, 23–27) | működik | DualShock 3: csak USB-n; Bluetooth-on nem |
| PS Move (28–29) | részben | csak USB-n, navigációra nem tesztelt |
| Hori / Nacon / Razer, `hid-generic` (22, 30–35) | működik, ha gamepadként jelentkezik | a „joystick” módú eszközök gombjai eltérhetnek; átállítható a beállításokban |
| Wii, `hid-wiimote` (36–39) | nem | csak Bluetooth-on csatlakoznak |
| Switch Pro, `hid-nintendo` (40) | működik | |
| Joy-Con, Switch Online kontrollerek (41–46) | nem | csak Bluetooth-on csatlakoznak |
| GameCube-adapter, Hori Switch padok (47–50) | működik | Mayflash adapter: `hid-mf` |
| Steam Controller / Steam Deck, `hid-steam` (51–52) | működik | kiosztás: `xbox-labels` |
| Logitech padok, kormányok, botok (53–65) | működik | Dual Action / RumblePad 2: `joystick` kiosztás; F310/F710 X módban `xpad` |
| Thrustmaster (66–75) | működik (alap funkciók) | T150: a teljes force feedbackhez közösségi driver kellene, nincs az image-ben |
| 8BitDo, fightstickek, egyéb (76–99) | működik | X-input módban `xpad`, D-input módban `hid-generic` |
| Generikus USB HID gamepad (100) | működik | DragonRise „USB Gamepad” (0079:0006): `joystick-playstation` kiosztás |

Bluetooth-támogatás (BlueZ és párosító képernyő) még nincs az image-ben. Valódi kontrollert eddig egyik csoportból sem teszteltünk; a bedugást és a gombkiosztást virtuális kontrollerekkel teszteljük (`tst_evdevhotplug`). Hardver nélküli kipróbálás, akár QEMU-ban futó image-en is: [controller-support.md](controller-support.md#testing-without-a-controller).

## Xbox-kompatibilis kontrollerek

| # | Kontroller | Linux driver | Linux driver link |
|---:|---|---|---|
| 1 | Microsoft Xbox Controller (eredeti) | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 2 | Microsoft Xbox Controller S | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 3 | Xbox 360 Wired Controller | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 4 | Xbox 360 Wireless Controller | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 5 | Xbox 360 Wireless Gaming Receiver | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 6 | Xbox One Controller | `xpad` / `xpadneo` BT-hez | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) / [xpadneo](https://github.com/atar-axis/xpadneo) |
| 7 | Xbox One S Controller | `xpad` / `xpadneo` BT-hez | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) / [xpadneo](https://github.com/atar-axis/xpadneo) |
| 8 | Xbox One X Controller | `xpad` / `xpadneo` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) / [xpadneo](https://github.com/atar-axis/xpadneo) |
| 9 | Xbox Elite Wireless Controller | `xpad` / `xpadneo` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) / [xpadneo](https://github.com/atar-axis/xpadneo) |
| 10 | Xbox Elite Series 2 | `xpad` / `xpadneo` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) / [xpadneo](https://github.com/atar-axis/xpadneo) |
| 11 | Xbox Series X\|S Controller | `xpad` / `xpadneo` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) / [xpadneo](https://github.com/atar-axis/xpadneo) |
| 12 | Xbox Adaptive Controller | `xpad` / `hid-generic` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 13 | Xbox Wireless Adapter for Windows | `xone` | [xone](https://github.com/medusalix/xone) |
| 14 | Xbox 360 Racing Wheel | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 15 | Xbox 360 Dance Pad | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 16 | PDP Afterglow Xbox Controller | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 17 | PDP Faceoff Wired Controller | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 18 | PowerA Enhanced Wired Controller | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 19 | PowerA Enhanced Wireless Controller | `xpad` / `hid-generic` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |
| 20 | Hyperkin Duke | `xpad` | [xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c) |

## PlayStation-kontrollerek

| # | Kontroller | Linux driver | Linux driver link |
|---:|---|---|---|
| 21 | Sony DualShock | `hid-sony` / adapterenként eltérő | [hid-sony.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c) |
| 22 | Sony DualShock 2 USB adapterrel | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 23 | Sony DualShock 3 | `hid-sony` | [hid-sony.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c) |
| 24 | Sony DualShock 4 | `hid-playstation` | [hid-playstation.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c) |
| 25 | Sony DualShock 4 Back Button Attachment | `hid-playstation` | [hid-playstation.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c) |
| 26 | Sony DualSense | `hid-playstation` | [hid-playstation.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c) |
| 27 | Sony DualSense Edge | `hid-playstation` | [hid-playstation.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c) |
| 28 | PlayStation Move Motion Controller | `hid-sony` | [hid-sony.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c) |
| 29 | PlayStation Move Navigation Controller | `hid-sony` | [hid-sony.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c) |
| 30 | Hori Fighting Commander PS | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 31 | Hori Real Arcade Pro | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 32 | Nacon Revolution Pro Controller | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 33 | Nacon Daija Arcade Stick | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 34 | Razer Raiju Tournament Edition | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 35 | Razer Wolverine V2 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |

## Nintendo-kontrollerek

| # | Kontroller | Linux driver | Linux driver link |
|---:|---|---|---|
| 36 | Nintendo Wii Remote | `hid-wiimote` | [hid-wiimote.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-wiimote.c) |
| 37 | Nintendo Wii Nunchuk | `hid-wiimote` | [hid-wiimote.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-wiimote.c) |
| 38 | Wii Classic Controller | `hid-wiimote` | [hid-wiimote.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-wiimote.c) |
| 39 | Wii U Pro Controller | `hid-wiimote` | [hid-wiimote.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-wiimote.c) |
| 40 | Nintendo Switch Pro Controller | `hid-nintendo` | [hid-nintendo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) |
| 41 | Nintendo Joy-Con L | `hid-nintendo` | [hid-nintendo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) |
| 42 | Nintendo Joy-Con R | `hid-nintendo` | [hid-nintendo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) |
| 43 | Nintendo Switch Online NES Controller | `hid-nintendo` | [hid-nintendo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) |
| 44 | Nintendo Switch Online SNES Controller | `hid-nintendo` | [hid-nintendo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) |
| 45 | Nintendo Switch Online N64 Controller | `hid-nintendo` | [hid-nintendo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) |
| 46 | Nintendo Switch Online Sega Genesis Controller | `hid-nintendo` | [hid-nintendo.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-nintendo.c) |
| 47 | Nintendo GameCube Controller USB adapterrel | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 48 | Hori Pokken Tournament Pro Pad | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 49 | Hori HORIPAD Switch | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 50 | Hori Fighting Stick Mini | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |

## Valve, Steam és PC gamepadok

| # | Kontroller | Linux driver | Linux driver link |
|---:|---|---|---|
| 51 | Valve Steam Controller | `hid-steam` | [hid-steam.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-steam.c) |
| 52 | Steam Deck beépített kontroller | `hid-steam` | [hid-steam.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-steam.c) |
| 53 | Logitech F310 | `hid-generic` / `xpad` mód | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 54 | Logitech F510 | `hid-generic` / `xpad` mód | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 55 | Logitech F710 | `hid-generic` / `xpad` mód | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 56 | Logitech Dual Action | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 57 | Logitech Rumblepad 2 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 58 | Logitech Chillstream | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 59 | Logitech G29 Racing Wheel | `hid-logitech-hidpp` / `hid-generic` | [hid-logitech-hidpp.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-logitech-hidpp.c) |
| 60 | Logitech G920 Racing Wheel | `hid-logitech-hidpp` / `hid-generic` | [hid-logitech-hidpp.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-logitech-hidpp.c) |
| 61 | Logitech G923 Racing Wheel | `hid-logitech-hidpp` / `hid-generic` | [hid-logitech-hidpp.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-logitech-hidpp.c) |
| 62 | Logitech Extreme 3D Pro | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 63 | Logitech Attack 3 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 64 | Logitech G Saitek X52 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 65 | Logitech G Saitek X56 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 66 | Thrustmaster T150 | `hid-generic` / közösségi driver | [T150 driver](https://github.com/scarburato/t150_driver) |
| 67 | Thrustmaster T300 RS | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 68 | Thrustmaster TX Racing Wheel | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 69 | Thrustmaster TMX | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 70 | Thrustmaster T248 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 71 | Thrustmaster T-GT | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 72 | Thrustmaster T.Flight HOTAS X | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 73 | Thrustmaster T.16000M | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 74 | Thrustmaster HOTAS Warthog | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 75 | Thrustmaster TCA Airbus Stick | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |

## 8BitDo, fightstickok és egyéb népszerű modellek

| # | Kontroller | Linux driver | Linux driver link |
|---:|---|---|---|
| 76 | 8BitDo SN30 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 77 | 8BitDo SN30 Pro | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 78 | 8BitDo SF30 Pro | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 79 | 8BitDo M30 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 80 | 8BitDo N30 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 81 | 8BitDo Pro 2 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 82 | 8BitDo Ultimate Wired | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 83 | 8BitDo Ultimate Bluetooth | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 84 | 8BitDo Ultimate 2C | `hid-generic` / Steam Input | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 85 | 8BitDo Zero 2 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 86 | Mayflash Magic-NS | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 87 | Mayflash F300 Arcade Stick | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 88 | Mayflash F500 Arcade Stick | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 89 | Mad Catz Street Fighter IV FightStick TE | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 90 | Mad Catz Street Fighter V Arcade FightStick TE2+ | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 91 | Qanba Obsidian Arcade Stick | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 92 | Qanba Dragon Arcade Stick | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 93 | GameSir G4 Pro | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 94 | GameSir T4 Pro | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 95 | Flydigi Vader 2 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 96 | Flydigi Apex 4 | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 97 | GuliKit KingKong 2 Pro | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 98 | SteelSeries Stratus Duo | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 99 | Google Stadia Controller | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |
| 100 | Generic USB / Bluetooth HID gamepad | `hid-generic` | [hid-generic.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-generic.c) |

## Fontos megjegyzések

- Az `xpad` kernelmodul az első- és harmadik féltől származó Xbox-kompatibilis kontrollerek széles körét támogatja; a nem felismert Xbox-modellek is általában „Generic Xbox controller”-ként működnek.
- PlayStationnél a `hid-playstation` a DualShock 4 és DualSense támogatásának fő kernelmeghajtója; a Nintendo Switch Pro Controller és Joy-Con eszközöket a `hid-nintendo` kezeli.
- Xbox One / Series kontrollernél Bluetooth kapcsolathoz gyakran jobb választás az `xpadneo`, mert rumble, akkumulátor-kijelzés és egységes gombhozzárendelés támogatást ad.
- A Linux input stack alapja az input/HID subsystem: a `usbhid`, `hid-generic` és `joydev` modulok a legtöbb szabványos USB gamepadhoz kellenek.

## Hasznos parancsok

```bash
# Csatlakoztatott input eszközök listázása
cat /proc/bus/input/devices

# USB eszközök listázása
lsusb

# HID eszközök és driverhozzárendelések
ls -l /sys/class/hidraw/

# Betöltött releváns kernelmodulok
lsmod | grep -E 'xpad|hid|joydev|playstation|nintendo|wiimote|steam'

# Kernelnapló a csatlakoztatott kontrollerhez
sudo dmesg | grep -iE 'input|hid|xbox|playstation|nintendo|joystick'
```

## Linux kernel driver források

- [Linux input subsystem dokumentáció](https://docs.kernel.org/input/input.html)
- [Linux kernel drivers mappa](https://github.com/torvalds/linux/tree/master/drivers)
- [xpad dokumentáció](https://github.com/torvalds/linux/blob/master/Documentation/input/devices/xpad.rst)
- [xpadneo](https://github.com/atar-axis/xpadneo)
- [xone](https://github.com/medusalix/xone)
- [Arch Wiki – Gamepad](https://wiki.archlinux.org/title/Gamepad)
