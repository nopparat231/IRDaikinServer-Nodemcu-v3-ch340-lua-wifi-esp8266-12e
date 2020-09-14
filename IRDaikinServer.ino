/*
* IRDaikinServer : Simple http interface to control Daikin A/C units via IR
* Version 0.1 July, 2017
* Copyright 2017 Andrea Cucci
*
* Dependencies:
*	- Arduino core for ESP8266 WiFi chip (https://github.com/esp8266/Arduino)
*	- IRremote ESP8266 Library (https://github.com/markszabo/IRremoteESP8266)
*
*/

#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ir_Daikin.h>
#include "IRDaikinServer.h"

#define DEBUG // If defined debug output is activated

#ifdef DEBUG
#define DEBUG_PRINT(x)		Serial.print(x)
#define DEBUG_BEGIN(x)		Serial.begin(x)
#define DEBUG_PRINTLN(x)	Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_BEGIN(x)
#define DEBUG_PRINTLN(x)
#endif 

IRDaikinESP daikinir(D2);  // An IR LED is controlled by GPIO pin 4 (D2)

const char* ssid = "Chanchai-fiber";
const char* password = "50480336";

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater; // Optional web interface to remotely update the firmware

setting_t fan_speeds[6] = { { "Auto", DAIKIN_FAN_AUTO },{ "Slowest", 1 },{ "Slow", 2 },{ "Medium", 3 },{ "Fast", 4 },{ "Fastest", 5 } };
setting_t modes[5] = { { "Cool", DAIKIN_COOL }, { "Heat", DAIKIN_HEAT }, { "Fan", DAIKIN_FAN }, { "Auto", DAIKIN_AUTO }, { "Dry", DAIKIN_DRY } };
setting_t on_off[3] = { { "On", 1 }, { "Off", 0 }, { "1HR Off", 0x012c } };

void saveStatus() {
	EEPROM_data data_new;
	data_new.temp = daikinir.getTemp();
	data_new.fan = daikinir.getFan();
	data_new.power = daikinir.getPower();
	data_new.powerful = daikinir.getPowerful();
	data_new.quiet = daikinir.getQuiet();
	data_new.swingh = daikinir.getSwingHorizontal();
	data_new.swingv = daikinir.getSwingVertical();
	data_new.mode = daikinir.getMode();
	EEPROM.put(0, data_new);
	EEPROM.commit();
}

void restoreStatus() {
	EEPROM_data data_stored;
	EEPROM.get(0, data_stored);
	if (data_stored.power != NULL) {
		daikinir.setTemp(data_stored.temp);
		daikinir.setFan(data_stored.fan);
		daikinir.setPower(data_stored.power);
		daikinir.setPowerful(data_stored.powerful);
		daikinir.setQuiet(data_stored.quiet);
		daikinir.setSwingHorizontal(data_stored.swingh);
		daikinir.setSwingVertical(data_stored.swingv);
		daikinir.setMode(data_stored.mode);
		daikinir.send();
	} else {
		daikinir.setTemp(25);
		daikinir.setFan(2);
		daikinir.setPower(0);
		daikinir.setPowerful(0);
		daikinir.setQuiet(0);
		daikinir.setSwingHorizontal(0);
		daikinir.setSwingVertical(0);
		daikinir.setMode(DAIKIN_COOL);
		daikinir.send();
		saveStatus();
	}
}

String getSelection(String name, int min, int max, int selected, setting_t* list) {
	String ret = "<select name=\""+name+"\">";
	for (int i = min; i <= max; i++) {
		ret += "<option ";
		if (list[i].value == selected)
			ret += "selected ";
		ret += "value = " + String(list[i].value) + " > " + String(list[i].name) + "</option>";
	}
	return ret += "</select><br\>";
}

void handleRoot() {
	String resp("");
	resp += "<html>" \
			"<head><title>IR Daikin Server</title><style type=\"text/css\"> * { -webkit-touch-callout: none; -webkit-user-select: none; box-sizing: border-box; font-size: 45; } *:before, *:after { box-sizing: border-box; font-size: 45; } html { background: linear-gradient(141deg, #0fb8ad 0%, #1fc8db 51%, #2cb5e8 75%); } html, body { margin: 0; padding: 0; width: 100%; height: 100%; overflow: hidden; font-family: serif; font-size: 45; } .remoter { width: 80%; height: 80%; background: #a3a3a3; border: 1px solid #817e75; box-shadow: 0 0 0 3px #cacbc5, 5px 3px 10px 6px rgba(0, 0, 0, 0.4); border-radius: 10px 10px 30px 30px; position: absolute; bottom: 10%; left: 10%; } .remoter:before, .remoter:after { content: \"\"; display: block; position: absolute; } .remoter:before { width: 16px; height: 12px; border-radius: 1px; z-index: -1; position: absolute; left: 30px; top: 0; -webkit-transition: top .2s; transition: top .2s; box-shadow: 0 0 20px 15px rgba(255, 0, 0, 0.7); } .remoter .btn { width: 60px; height: 60px; border-radius: 50%; border: 3px solid #2d2f2e; margin: 10px 0 0 10px; position: relative; } .remoter .btn:hover { cursor: pointer; border-color: #464948; } .remoter .btn.active { border-color: #5f6361; } .remoter .dark { background: #2d2f2e; } .remoter .icon { font-size: 2em; font-weight: bold; position: absolute; top: 50%; left: 50%; -webkit-transform: translate(-50%, -50%); transform: translate(-50%, -50%); -webkit-transform: translate(-50%, -50%) scale(0.6); transform: translate(-50%, -50%) scale(0.6); } .remoter .icon a { text-decoration: none; color: #2d2f2e; } .remoter .dark .icon a { text-decoration: none; color: #a3a3a3; } .remoter .vol-label { font-size: 12px; font-weight: bold; position: absolute; top: 230px; right: 15px; -webkit-transform: scale(0.7); transform: scale(0.7); } </style></head>" \
			"<body>" \
			"<div>" \
			"<div class=\"remoter\">" \
			"<h1>IR Daikin Server</h1>" \
			"<form method=\"POST\" action=\"cmd\">";
	resp += "Power: " + getSelection("power", 0, 1, daikinir.getPower(), on_off);
	resp += "Temperature: <select name=\"temp\">";
	for (int i = DAIKIN_MIN_TEMP; i <= DAIKIN_MAX_TEMP; i++) {
		resp += "<option ";
		if (i == daikinir.getTemp())
			resp += "selected ";
		resp += ">" + String(i) + "</option>";
	}
	resp += "</select><br\>";
	resp += "Mode: " + getSelection("mode", 0, 5, daikinir.getMode(), modes);
	resp += "Fan speed: " + getSelection("fan", 0, 5, daikinir.getFan(), fan_speeds);
	resp += "Powerful Mode: " + getSelection("powerful", 0, 1, daikinir.getPowerful(), on_off);
	resp += "Quiet Mode: " + getSelection("quiet", 0, 1, daikinir.getQuiet(), on_off);
	resp += "Horizontal Swing: " + getSelection("swingh", 0, 1, daikinir.getSwingHorizontal(), on_off);
	resp += "Vertical Swing: " + getSelection("swingv", 0, 1, daikinir.getSwingVertical(), on_off);
	resp += "<input type=\"submit\">" \
			"</form></div></div>" \
			"</body>" \
			"</html>";
	server.send(200, "text/html", resp);
}

void handleCmd() {
	String argName;
	uint8_t arg;
	for (uint8_t i = 0; i < server.args(); i++) {
		argName = server.argName(i);
		arg = strtoul(server.arg(i).c_str(), NULL, 10);
		if (argName == "temp") { daikinir.setTemp(arg); } 
		else if (argName == "fan") { daikinir.setFan(arg); }
		else if (argName == "power") { daikinir.setPower(arg); }
		else if (argName == "powerful") { daikinir.setPowerful(arg); }
		else if (argName == "quiet") { daikinir.setQuiet(arg); }
		else if (argName == "swingh") { daikinir.setSwingHorizontal(arg); }
		else if (argName == "swingv") { daikinir.setSwingVertical(arg); }
		else if (argName == "mode") { daikinir.setMode(arg); }
		DEBUG_PRINT(argName);
		DEBUG_PRINT(" ");
		DEBUG_PRINTLN(arg);
	}
	saveStatus();
	daikinir.send();
	handleRoot();
}

void handleNotFound() {
	server.send(404, "text/plain", "404 File Not Found");
}

void setup(void) {	
	EEPROM.begin(EEPROM_SIZE);
	DEBUG_BEGIN(115200);

	daikinir.begin();
	restoreStatus();

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	DEBUG_PRINTLN("");

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		DEBUG_PRINT(".");
	}
	DEBUG_PRINTLN("");
	DEBUG_PRINT("Connected to ");
	DEBUG_PRINTLN(ssid);
	DEBUG_PRINT("IP address: ");
	DEBUG_PRINTLN(WiFi.localIP());

	server.on("/", handleRoot);
	server.on("/cmd", handleCmd);

	server.onNotFound(handleNotFound);

	httpUpdater.setup(&server);
	server.begin();
	DEBUG_PRINTLN("HTTP server started");

	if (MDNS.begin("daikin", WiFi.localIP())) {
		DEBUG_PRINTLN("MDNS responder started");
	}
}

void loop(void) {
	server.handleClient();
}
