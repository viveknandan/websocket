const char landing_page[]= R"rawliteral(
<!DOCTYPE HTML><html>
<body>
  <h1>Welcome to Smart Socket Landing .</h1>
  <br><p style="color:green;">To continue please <a href="https://192.168.4.1/login">login</a></p>
</body>
</html>
)rawliteral";
const char login_page[]= R"rawliteral(
<!DOCTYPE html><html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
body {font-family: Arial, Helvetica, sans-serif;}
form {border: 3px solid #f1f1f1;}

input[type=text], input[type=password] {
  width: 100%;
  padding: 12px 20px;
  margin: 8px 0;
  display: inline-block;
  border: 1px solid #ccc;
  box-sizing: border-box;
}

button {
  background-color: #04AA6D;
  color: white;
  padding: 14px 20px;
  margin: 8px 0;
  border: none;
  cursor: pointer;
  width: 100%;
}

button:hover {
  opacity: 0.8;
}

.cancelbtn {
  width: auto;
  padding: 10px 18px;
  background-color: #f44336;
}

.imgcontainer {
  text-align: center;
  margin: 24px 0 12px 0;
}

img.avatar {
  width: 40%;
  border-radius: 50%;
}

.container {
  padding: 16px;
}

span.psw {
  float: right;
  padding-top: 16px;
}

/* Change styles for span and cancel button on extra small screens */
@media screen and (max-width: 300px) {
  span.psw {
     display: block;
     float: none;
  }
  .cancelbtn {
     width: 100%;
  }
}
</style>
</head>
<body>

<h2>Login</h2>

<form action="/login" method="get">
  <div class="imgcontainer">
  </div>

  <div class="container">
    <label for="uname"><b>Username</b></label>
    <input type="text" placeholder="Enter Username" name="uname" required>

    <label for="psw"><b>Password</b></label>
    <input type="password" placeholder="Enter Password" name="psw" required>
        
    <button type="submit">Login</button>
    <label>
      <input type="checkbox" checked="checked" name="remember"> Remember me
    </label>
  </div>

  <div class="container" style="background-color:#f1f1f1">
    <button type="button" class="cancelbtn">Cancel</button>
    <span class="psw">New <a href="/newuser">User?</a></span>
  </div>
</form>

</body>
</html>
)rawliteral";
const char welcome_page[]= R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <style>
  span.psw {
  float: left;
  padding-top: 16px;
  }
  </style>
 <meta name="viewport" content="width=device-width, initial-scale=1.0">
</head>
<body>

<h1>Discover Devices</h1>
<br>
<button type="button" onclick="discoverDevices()" id="discover">Discover</button>
<br>
<br/><br/>
<ul id="devicelist">
</ul>
  <form id="select_device_form">
    <label for="select_device_form">DeviceId:</label><br>
    <input type="number" id="select_device_id" name="select_device_id" min="0" max=50><br>
    <button type="button" onclick="selectDevice()">Set</button>
  </form>
<h1>Select Device</h1>
<br>
<button type="button" onclick="selectDevice()" id="select">SelectDevice</button>
<br>

<h1>Select Relay Operation</h1>
<br>
<button type="button" onclick="relayOn()" id="relayon">PowerOn</button>
<br>
<br>
<button type="button" onclick="relayOff()" id="relayoff">PowerOff</button>
<br>
<p id="RelayStateOn" style="color:green;"></p>
<br>
<p id="RelayStateOff" style="color:red;"></p>
<br>
  <h1>Measurements</h1>
  <form id="measurement_interval">
    <label for="interval">Measurement Interval in Seconds:</label><br>
    <input type="number" id="interval" name="interval" min="0" max=86400><br>
    <button type="button" onclick="setInterval()">Set</button>
  </form>
<p id="vrms" ></p>
<p id="irms" ></p>
<p id="pf" ></p>
<p id="freq" ></p>
<p id="Q" ></p>
<p id="S" ></p>
<p id="P" ></p>
<button type="button" onclick="startstopMeasure()" id="measure">Start Measurement</button>
<br>
<br>

 
<p style="color:blue;">MAC Address: </p>
<p id="MAC"           style="color:black;">MAC Address:</p>
<br>
<p style="color:blue;">IP Address: </p>
<p id="IP"            style="color:black;">IP:</p>
<br>
<span class="psw"><a href="/logout">logout</a></span>
<script>
  var gateway = `wss://192.168.4.1/ws`;
  var websocket;
  var interval=1;
  var isMeasureStarted = false;
  var counter= 1;
  var current_device = 0;
  const Commands = Object.freeze({ 
    RELAY_ON: 0, 
    RELAY_OFF: 1, 
    START_MEASURE: 2, 
    STOP_MEASURE: 3, 
    SET_TIME: 4, 
    START_LOG: 5,
    STOP_LOG: 6,
	DISCOVER: 7,
	SELECTDEVICE: 8
});
  const CommandsType = Object.freeze({ 
    INTEGER: 0, 
    DOUBLE: 1, 
    STRING: 2	
}); 
  window.addEventListener('load', onLoad);
  
  function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
	if(websocket)
	{
	    websocket.onopen    = onOpen;
		websocket.onclose   = onClose;
		websocket.onmessage = onMessage; // <-- add this line
	}
	else
	{
	    console.log('Trying to open a WebSocket connection... failed');
	}

  }
  function onOpen(event) {
    console.log('Connection opened');
  }
  function onClose(event) {
    console.log('Connection closed');
    setTimeout(initWebSocket, 2000);
  }
  function onMessage(event) {
    var state;
    var obj ;

	console.log('Incoming message:'+event.data);
	try
	{
	      obj = JSON.parse(event.data);
    }
	catch(error)
	{
	   //Plain message
	}
	if(obj)
	{
	
	    if (obj.objectId == "1")
		{
		  //History
		}
		else if(obj.objectId == "2")
		{
		  //Info
		  console.log("Object Id 2 received");
		  current_device = obj.deviceid;
		  if(obj.relay=="1")
		  {
			document.getElementById('RelayStateOn').innerHTML = "Relay ON";
			document.getElementById('RelayStateOff').innerHTML = "";
		  }
		  else if(obj.relay=="0")
		  {
			document.getElementById('RelayStateOn').innerHTML = "";
			document.getElementById('RelayStateOff').innerHTML = "Relay OFF";
		  }

		  document.getElementById('MAC').innerHTML = obj.mac;
		  document.getElementById('IP').innerHTML = obj.ip;
		  button = document.getElementById('measure');
		  irmsobj = document.getElementById('irms');
		  vrmsobj = document.getElementById('vrms');
		  pfobj = document.getElementById('pf');
		  Qobj = document.getElementById('Q');
		  Sobj = document.getElementById('S');
		  Pobj = document.getElementById('P');
		  Freqobj = document.getElementById('freq');
		  if(obj.measure_active=="1")
		  {
		      
			  button.innerHTML = "Stop Measurement";
			  isMeasureStarted = true;
			 irmsobj.innerHTML='RMS Voltage(Volts):'+obj.vrms;
			 vrmsobj.innerHTML='RMS Current(Amps):'+obj.irms;
			 pfobj.innerHTML='Power Factor:'+obj.pf;
			 Qobj.innerHTML='Average ReactivePower(Watt):'+ obj.Q;
			 Sobj.innerHTML='Average ActivePower(Watt):' + obj.S;
			 Pobj.innerHTML='Average Power(Watt):'+obj.P;
			 Freqobj.innerHTML='Freq(Hz):'+obj.freq;
		  }
		  else
		  {
		     button.innerHTML = "Start Measurement";
			 isMeasureStarted = false;
			 irmsobj.innerHTML='';
			 vrmsobj.innerHTML='';
			 pfobj.innerHTML='';
			 Qobj.innerHTML='';
			 Sobj.innerHTML='';
			 Pobj.innerHTML='';
			 Freqobj.innerHTML='';
		  }
		}
		else if(obj.objectId == "3")
		{
		  addelement(obj.deviceid,obj.groupid,obj.devicename)
		}

	}

  }
  
function addelement(deviceId,group, devicename) {
var completelist= document.getElementById("devicelist");

completelist.innerHTML += "<li>Device " + counter +":"+deviceId+","+"group : "+group+"Name:"+devicename+ "</li>";
counter++;
}
function selectDevice()
{
	const messageInput = document.getElementById('select_device_id');
	cmd = Commands.SELECTDEVICE+":"+id;
	websocket.send(cmd);
}

  function discoverDevices()
  {
    cmd = Commands.RELAY_ON;
    websocket.send(cmd);
	
  }
  function onLoad(event) {
    initWebSocket();

  }
  function toggle(){
    websocket.send('toggle');
  }
  function relayOn(){
    cmd = Commands.RELAY_ON;
    websocket.send(cmd);
  }
  function relayOff(){
    cmd = Commands.RELAY_OFF;
    websocket.send(cmd);
  }
  function setInterval()
  {
	const messageInput = document.getElementById('interval');
	interval = messageInput.value;
	console.log('interval set to'+interval);
  }
  function setTime(val)
  {
     cmd = Commands.SET_TIME+':'+val+','+CommandsType.STRING;
     websocket.send(cmd);
  }
  

  function startstopMeasure()
  {
    button = document.getElementById('measure');
	if(isMeasureStarted)
	{
	    //Stop Measurement
		
		button.innerHTML = "Stop Measurement";
		cmd = Commands.STOP_MEASURE;
		isMeasureStarted = false;
	}
	else
	{
	   	//Start Measurement
		button.innerHTML = "Start Measurement";
		cmd = +Commands.START_MEASURE+':'+(interval).toString()+','+CommandsType.INTEGER;
		isMeasureStarted = true;
	}
	 websocket.send(cmd);
	console.log('Sent Command:'+cmd);
  }
</script>

</body>
</html>
)rawliteral";

const char message[] = R"rawliteral(
  {"objectId":"%d",
  "deviceid": "%d",
  "groupid": "%d",
  "devicename": "%s",
  "relay":"%d",
  "mac":"%s",
  "ip":"%s",
  "vrms":"%.2f",
  "irms":"%.2f",
  "pf": "%.2f",
  "freq": "%.2f",
  "Q": "%f",
  "S":"%.2f",
  "P":"%.2f",
  "time":"%d",
  "measure_active":"%d"
})rawliteral";


#ifdef __cplusplus
extern "C"
{
#endif
const char* getLandingPage()
{
    return landing_page;
}

const char* getWelcomePage()
{
    return welcome_page;

}

const char* getLoginPage()
{
    return login_page;
}

const char* getMessage()
{
  return message;
}
#ifdef __cplusplus
}
#endif