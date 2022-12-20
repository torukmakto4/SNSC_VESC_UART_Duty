(c) torukmakto4 and released permanently under CC BY-SA 4.0
This code is example of a properly behaving and mostly refined voltage command throttle with a VESC if you hate current control, like me.
As is, it does basic vehicle management for most ebike/scooter type things.
Brake control is current command. Also, there is taillight control (car style brake lights uing LFPWM dimming) and throttle/brake safety interlock.
Adjust endpoints for analog inputs as necessary, controls/Halls may vary. Adjust constraints on current commands to fit your VESC setup.
Tested with Arduino IDE 1.8.19 and targets the ATmega328 (P or not doesn't matter) - might also fit a couple other similar AVRs.
It doesn't matter what board as long as it doesn't interfere with any pins needed. I use a custom PCB, you can use a cheap Pro Mini style thing.
Pinouts are, well, figure them out. The AVR's hardware UART is used to talk to the inverter and the throttle and brake halls are
on ADC6/7. Arduino pin "10" is an active high taillight drive, use a single-ended powerstage and a current source/resistance to drive LED.
Configure the UART channel on the VESC for 250K baud (or other values) to align with the standard 16.000MHz clock on the AVR.
Certain other baud rates will have too much error and not work without changing the crystal or resonator.
This requires the SolidGeek VescUart library https://github.com/SolidGeek/VescUart .
