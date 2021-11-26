
Still Working On this But Code Is working and pulling Data through to Hassio via sensor:
Im using a D1mini but will work on esp8266 

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
 
```
```
subs:
  
#define HASSIOHRVSTATUS "hassio/hrv/status"
#define HASSIOHRVSUBHOUSE "hassio/hrv/housetemp"
#define HASSIOHRVSUBROOF "hassio/hrv/rooftemp"
#define HASSIOHRVSUBCONTROL "hassio/hrv/controltemp"
#define HASSIOHRVSUBFANSPEED "hassio/hrv/fanspeed"

// MQTT pubs Testing Not yet functional 
#define HASSIOTESTINGPOWERCONTROL "hassio/hrv/powercontrol"
#define HASSIOTESTINGBURNTTOASTMODE "hassio/hrv/burnttoast"
#define HASSIOTESTINGVENTERLATIONLEVEL "hassio/hrv/venterlationlevel"
#define HASSIOTESTINGTEMPCONTROL "hassio/hrv/tempcontol"
```
at the moment there are no controlls e.g tuning on off or setting fan speeds and on off times / burnt toast mode hopfully I can get comands to work..
![HRV Hassio](https://user-images.githubusercontent.com/29391962/141737219-631d36ff-4ed0-4e42-ac0c-32908596b6b3.png)
![HRV ESP Hassio](https://user-images.githubusercontent.com/29391962/142518413-146c2566-e426-4eaf-9b21-13e86fca7d52.png)


