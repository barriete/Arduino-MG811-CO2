# Arduino-MG811-CO2
Arduino Project to measure CO2 concentration in the air and upload the data to cloud

# Elements
1. Arduino MKR1010 Wifi
2. MG811 CO2 Sensor
3. DHT22 Temperature & Humidity Sensor
4. Power Supply (6V)

# To be taken into account
MKR1010 Analog Input Pin Resolution is 3.3v that means 4096
MG811 is highly affected by humidity


# References
If you go through the code you will quickly notice there are several pieces of code copied and pasted. Some of them have been taken from this code:
https://gist.github.com/yiiju
https://github.com/smart-tech-benin/MG811
https://github.com/solvek/CO2Sensor
https://gist.github.com/ic/6412a4354304665e05d5951912c2cbfb

Special thanks to all of their authors for contributing to the best understanding of this sensor

# See also:
[Calibration notes from CO2 Meter](https://www.co2meter.com/blogs/news/7512282-co2-sensor-calibration-what-you-need-to-know)
[Sandbox Electronics' MG-811 CO2 Sensor Module](http://sandboxelectronics.com/?p=147)
[Vernier's post on human respiration](http://www.vernier.com/innovate/human-respiration/)
