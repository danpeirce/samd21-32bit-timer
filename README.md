SAMD21 32bit timer.

This arduino program will enable a 32bit timer on a SAMD21 by combining TC Counters 4 & 5.
The SAMD21 is configured to load the CC0 and CC1 with pulse and period measurements from a single pin.
My purpose for creating this was to trim oscillators and measure longer durations.
This arduino file will work on both the Seeeduino XIAO and Adafruit Trinket M0 (tested on both).
Once programmed a serial port needs to be opened to the device before the measurement is made.
An optional delay is included after a single period has been measured (as to not overwhelm the serial port).

I highly recommend the Seeedino Xiao over the Trinket M0 as it is cheaper and has more pins.
I'm not a big fan of the USB C connector on the Xiao (I prefer the Trinket M0's micro USB), as I needed to buy more C type cables.
My Seedino Xiao spends most of the timer connected to my Raspberry Pi (as the Pi does not have any real-time measuring capability). 

Why use a 32bit timer instead of a 16bit one? A 32bit timer can measure a period/pulse of 89.5 seconds at maximum precision without overflowing ( 20.8ns * 2 ^ 32 ).
What can a 16bit timer measure (without overflowing, at maximum precision)... 1.36 milliseconds.

There is no license for this work, it is use-at-your-own-risk (I assume no liability).
Many parts of this program came from on-line forums. I included the URLs in the program header to give credit.
