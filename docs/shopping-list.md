# Shopping list

Links and prices as found on 12 November 2026, for the capacitor, button and buzzer on 15 November 2026.
Prices include VAT unless noted, shipping is extra.
Shops were chosen for delivery to Germany; every part is a standard item, so any other source works.
Links to marketplace listings can go stale; the part names are what matters.

## Electronics (all plug-in, no soldering)

| Qty | Part | Where | Price seen |
|---|---|---|---|
| 1 | Seeed Studio XIAO ESP32-C6, **pre-soldered** (Seeed SKU 102010636) | [BerryBase](https://www.berrybase.de/seeed-xiao-esp32-c6-wi-fi-6-ble-5.0-zigbee-thread-512kb-sram-4mb-flash-uart-spi-risc-v-019f466b67fa700690e84a8dff9cbf19) (article SE-102010636) · [Kiwi Electronics](https://www.kiwi-electronics.com/en/brand-seeed-studio/seeed-studio-xiao-esp32c6-pre-soldered-20410) | about 5.70-6.00 EUR ex. VAT at Kiwi |
| 1 | 28BYJ-48 stepper, 5 V, with ULN2003 driver board | [BerryBase](https://www.berrybase.de/28byj-48-schrittmotor-5v-dc-uln2003a-treiberplatine) · [Funduinoshop](https://funduinoshop.com/en/electronic-modules/electric-motors-and-accessories/elektromotoren/stepper-motors/stepper-motor-28byj-48-with-uln2003-driver-board) | 2.89 EUR at Funduinoshop |
| 1 | MG90S micro servo, metal gear, 180 degrees | [Funduinoshop](https://funduinoshop.com/en/electronic-modules/electric-motors-and-accessories/elektromotoren/servo-motors/mg90s-metal-gear-micro-servo) · [AZ-Delivery](https://az-delivery.de/en/collections/aktoren/products/mg90s-micro-servomotor) | 4.00 EUR at Funduinoshop |
| 2 | IR break-beam sensor pair, 3 mm LEDs, wires with header ends (Adafruit 2167) | [Opencircuit (NL)](https://opencircuit.shop/product/ir-break-beam-sensors-premium-wire-header-ends) · [Adafruit](https://www.adafruit.com/products/2167), which names Eckstein Komponente as its German distributor · [Melopero (IT)](https://www.melopero.com/en/shop/sensors/infrared/irbreakbeamsensor3mmleds/) | 2.10 EUR each at Opencircuit |
| 1 | Mini breadboard, 170 contacts | [BerryBase](https://www.berrybase.de/en/mini-breadboard-with-170-contacts) | from 0.99 EUR |
| 1 | Electrolytic capacitor 470 uF, 16 V or more, radial, up to 10 mm diameter (the BerryBase one is 25 V, 10 x 16 mm) | [BerryBase](https://www.berrybase.de/elektrolytkondensator-470-f-25v-1050c-1-radial-tht) | 0.10 EUR |
| 1 | Jumper wire set with male-male and female-male wires, 20 cm ("40pin Jumper / Dupont Kabel Set, je 1x F-F / M-M / F-M") | [BerryBase, prototyping category](https://www.berrybase.de/bauelemente/prototyping/) | 4.90 EUR |
| 1 | USB-C power supply 5.1 V / 3 A with fixed cable (official Raspberry Pi 15 W supply) | [BerryBase](https://www.berrybase.de/offizielles-raspberry-pi-usb-c-netzteil-5-1v-3-0a-eu-schwarz) | 7.90 EUR |

At these prices the electronics come to roughly 32 EUR plus shipping. Everything except the servo
and the light barriers is listed at BerryBase, so most of it fits in one order.

The capacitor is needed: it keeps the servo's starting current from resetting the ESP32. The servo
cable does not reach the breadboard on its own, but no extension lead is needed: three male-male
jumpers from the set plug straight into the servo's plug and bridge the rest. Jumper wires used:
9 male-male (6 of them as short links on the breadboard, where pre-cut breadboard jumpers are
neater, and 3 for the servo) and 6 female-male, so one set is plenty.

Optional, for the holes in the side walls and for the beeps:

| Qty | Part | Where | Price seen |
|---|---|---|---|
| 1 or 2 | Push button, 12 mm panel mount, momentary, 15 cm leads with 2.54 mm Dupont plugs ("KKSB 12mm Druckknopf Momentanschalter") | [BerryBase](https://www.berrybase.de/en/kksb-12mm-druckknopf-momentanschalter-mit-2.54mm-dupont-anschluessen-150mm-kabel) | 4.90 EUR each |
| 1 | Active buzzer (it has its own oscillator), 12 mm, 3-7 V DC, 2.3 kHz, 85 dB, type SG37D2K3 | [BerryBase](https://www.berrybase.de/signalgeber-oe12-0mm-3-7v-dc-2-3khz-85db) | 0.75 EUR |

Any 12 mm push button that closes when pressed works. One person needs one; two people sharing the
dispenser get one each, and each button releases the doses of its own profile (right: profile 1,
left: profile 2). The leads have to reach the breadboard at the back: the KKSB ones are 15 cm, so
extend each with a jumper from the set (male-male if the leads end in sockets, female-male if they
end in pins).

The buzzer hangs straight on a pin of the XIAO, + to D5. The firmware sets that pin to its strongest
drive, which Espressif rates at 40 mA (the default is 20 mA), so any active buzzer for 3.3 V that
draws 30 mA or less works. BerryBase gives 30 mA for this one without saying at which voltage; at
3.3 V it draws less. Its legs take two female-male jumpers; + is marked on the buzzer and is usually
the longer leg.

Before ordering the base print: the sensor mounts assume housings of 20 x 10 x 8 mm, the size
Adafruit gives for part 2167. Clones can differ.

## Hardware

| Qty | Part | Standard | Where |
|---|---|---|---|
| 2 | Ball bearing 608 (8 x 22 x 7 mm), ZZ or 2RS | - | [eBay, G.A.P Kugellager Shop](https://www.ebay.de/itm/261993530414) (3.50 EUR seen) |
| 1 | Hex bolt M8 x 80 | DIN 931 or DIN 933 | hardware store, sold singly; or as a set with nut and washer from [this eBay shop](https://www.ebay.de/str/gewindewerk) (choose M8 x 80) |
| 1 | Self-locking nut M8 | DIN 985 | [Hornbach](https://www.hornbach.de/s/Mutter%20M8) |
| 1 | Washer M8 | DIN 125 | hardware store |
| 2 | Socket cap screw M3 x 10 (stepper) | DIN 912 / ISO 4762 | [Accu](https://accu-components.com/de/metrische-zylinderkopfschrauben/3819-SSCF-M3-10-A2) |
| 4 | Socket cap screw M3 x 12 (tower) | DIN 912 / ISO 4762 | [Accu](https://accu-components.com/de/metrische-zylinderkopfschrauben/3820-SSCF-M3-12-A2) |
| 10 | 60 mm pieces of 1.75 mm filament (keeper pins) | - | cut from the print filament |
| 4 | Stick-on rubber feet (optional) | - | hardware store |

## Filament

About 1.6 kg of PETG for the full set with 10 modules (two 1 kg spools), about 0.45 kg for base,
carousel and station alone. PLA works too.

Print the carousel and the chute in black or another dark, opaque colour: their index flags and walls
must block the infrared light of the light barriers. Everything else can be any colour.
