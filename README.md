# AtomMatrix_Wigler
AtomGPS code with additions to do silly things with the AtomMATRIX display unit

# 4/7/24 someday I'll spend timing learning Git, just not today
I added another version "14" that is built from the latest @lukeswitz v1.40b3 release
- optimized the LED lights code to make it better reflect the activity, and speed it up
- I am now seeing performance on-par with a plain AtomGPS unit running Luke's code
- 

This is NOT a proper fork from other's code... I need to do that at some point...

There are three flags in the code that decide which flashy-lights thing happens:
showChannelLED = true;      // flash the LED corresponding to found channel number
showChannelNumber = false;  // show the Channel number of each channel as found
showScanCount = false;      // show the number of networks found in each scan pass 

You could set them ALL to True, but that is just chaos-light-show.

To Do:
- Need a config file for the light show options
- Or better yet, use the button to toggle through them
- Characters that don't overlap with the GPS VU meter
- I'm sure there are MILLION ways to do the lights smarter and faster...
- Following @lozaning's example, put the display code on one core and the scan code on the other (they make it look easy!)
- 
