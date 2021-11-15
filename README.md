
Still Working On this But Code Is working and pulling Data through to Hassio via sensor:

sensor.yaml
```
- platform: mqtt                          
  state_topic: "hassio/hrv/status"
  name: "HRV Status"
  unit_of_measurement: ""
  
- platform: mqtt                          
  state_topic: "hassio/hrv/housetemp"
  name: "House Temp"
  unit_of_measurement: ""
  
- platform: mqtt                          
  state_topic: "hassio/hrv/rooftemp"
  name: "Roff Temp"
  unit_of_measurement: ""
  
- platform: mqtt                          
  state_topic: "hassio/hrv/fanspeed"
  name: "Fan Speed"
  unit_of_measurement: ""

- platform: mqtt                          
  state_topic: "hassio/hrv/controltemp"
  name: "Control Temp"
  unit_of_measurement: ""
#define OPENHABHRVSTATUS "hassio/hrv/status"
#define OPENHABHRVSUBHOUSE "hassio/hrv/housetemp"
#define OPENHABHRVSUBROOF "hassio/hrv/rooftemp"
#define OPENHABHRVSUBCONTROL "hassio/hrv/controltemp"
#define OPENHABHRVSUBFANSPEED "hassio/hrv/fanspeed"
```
![HRV Hassio](https://user-images.githubusercontent.com/29391962/141737219-631d36ff-4ed0-4e42-ac0c-32908596b6b3.png)

