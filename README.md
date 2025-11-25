# SMS Plus for PicoCalc

This is a port of Charles MacDonald's [SMS Plus](https://segaretro.org/SMS_Plus) for the [PicoCalc](https://www.clockworkpi.com/product-page/picocalc)

Currently in quite early stages, but so far sound and input should be working well enough

## Compiling notes

Pico1 is very limited in RAM, so you either need to use `BAKED_ROM` or lose SRAM. Or upgrade to a Pico2 :)

`BAKED_ROM` expects a file `rom.h` providing a `const uint8_t sms_rom` containing your ROM in the root of the project

### rp2040-psram

apply patch: https://gist.github.com/marc-hanheide/77d2685ceb2aaa4b90324c520dd4c34c

## A footnote

I don't like the GNU Foundation or their license, however SMS Plus is licensed under GPL Version 2, and so this repository is licensed as necessary, under protest.