#ifndef _APPLICATION_H_
#define _APPLICATION_H_

#include <Arduino.h>
#include "core/CommStack.h"
#include <FS.h>
#include "event_logger.h"
#include "MK20.h"
#include "errors.h"

class Mode;

#define LOG(m) //EventLogger::log(m)//Serial.println((m)) //Logger.println(logString(m));
#define LOG_VALUE(m,v) //EventLogger::log((m))//;Serial.println(v)//Logger.print(logError(m));Logger.println(v);

struct FirmwareUpdateInfo {
	int buildnr;
	String mk20_url;
	String esp_url;
	String mk20_ui_url;
};

class ApplicationClass: CommStackDelegate
{

public:
	ApplicationClass();
	~ApplicationClass();

	bool firmwareUpdateNotified() { return _firmwareChecked; };
	bool isMK20Available() { return _mk20OK; };
	void pingMK20();
    void updateMK20Firmware();
    int getBuildNumber() { return _buildNumber; }
	void loop();
	void setup();
	void pushMode(Mode* mode);
	Mode* currentMode() { return _currentMode; };
	MK20* getMK20Stack() const { return _mk20; };
	float getDeltaTime();

	void sendPulse(int length=5, int count=1);

	void reset();

	void idle();
	void handleError(DownloadError error);

	void setFirmwareUpdateInfo(FirmwareUpdateInfo* info) { _firmwareUpdateInfo = info; };
	bool firmwareUpdateAvailable() { return _firmwareUpdateInfo != NULL; };
	FirmwareUpdateInfo* getFirmwareUpdateInfo() { return _firmwareUpdateInfo; };

public:
	bool runTask(CommHeader& header, const uint8_t* data, size_t dataSize, uint8_t* responseData, uint16_t* responseDataSize, bool* sendResponse, bool* success);

private:
	bool _firstModeLoop;
	Mode *_nextMode;
	Mode *_currentMode;
	unsigned long _lastTime;
	float _deltaTime;
	unsigned long _buttonPressedTime;
	MK20* _mk20;
	bool _mk20OK;
    unsigned long _appStartTime;
	unsigned long _lastMK20Ping;
//	WiFiServer _server;
    int _buildNumber;
	FirmwareUpdateInfo* _firmwareUpdateInfo;
    bool _firmwareChecked;
};

extern ApplicationClass Application;

#endif //_APPLICATION_H_
