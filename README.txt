"# Embedded-Systems-Final" 

- This project was created for my Embedded Systems Design class at Auburn University.
- It has two purposes; 
	- to execute an interrupt-driven 1-second up/down counter
	- to drive a DC motor with or without tachometer feedback.

- Main.c is a program for the NUCLEO-L432, a development board for the STM32L432KCU microcontroller.
- Pins and interrupts are defined, the motor is callibrated to determine max speed, then the program waits
  for a system interrupt or for the motor driver to enter constant speed mode.
- Counter is connected to LEDs on pins PB3:PB6
- Motor Driver PWM output from PA0 drives gate of N-channel MOSFET, connecting DC motor to 15V

- System control is done with a 4x4 matrix keypad.
	- '*' starts and stops the counter (default = stopped)
	- '#' changes counter direction (default = up)
	- Motor driver operates in constant duty cycle mode by default
		- '0' -> '9', 'A' set the duty cycle to 0%, 10%, ... 90%, 100%
		- Motor driver does not account for load on the motor, so actual speed may vary
	- Motor driver operates in constant speed mode by pressing 'D'
		- The target speed defaults to 50% of the max speed determined in motor callibration.
		- The tachometer reading is a function of motor speed.
		- The tachometer reading is rectified, filtered, and brought within logic level voltage. 
		- The built-in ADC converts voltage reading to digital value
		- A scalar multiple of the difference in target speed and actual speed is subtracted from or
		  added to the PWM Duty Cycle, depending on if the actual speed is greater or less than the
		  target.
		- This way, as the actual speed gets closer to the target speed, the corrections become smaller
		  and smaller and the output becomes stable.
