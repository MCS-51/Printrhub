//
// Created by Phillip Schuster on 09.07.16.
//

#include "Application.h"
#include "SceneController.h"
#include "../animation/Animator.h"
#include "../../scenes/DownloadFileController.h"
#include "../../scenes/alerts/ErrorScene.h"
#include "../../scenes/projects/ProjectsScene.h"
#include "../../scenes/firmware/ConfirmFirmwareUpdateScene.h"
#include "Printr.h"
#include <ArduinoJson.h>
#include "../../errors.h"
#include "../../jobs/ReceiveSDCardFile.h"

ApplicationClass Application;

extern Printr printr;

ApplicationClass::ApplicationClass()
{
	_firstSceneLoop = true;
	_touched = false;
	_nextScene = NULL;
	_currentScene = NULL;
	_lastTime = 0;
	_deltaTime = 0;
  _buildNumber = FIRMWARE_BUILDNR;
	_esp = new CommStack(&Serial3,this);
  _espOK = false;
  _lastESPPing = 0;
  _currentJob = NULL;
}

ApplicationClass::~ApplicationClass()
{

}


void ApplicationClass::handleTouches()
{
	//If we don't have a screen controller we don't have to handle touches
	if (_currentScene == NULL) return;

	//Get current scene controller
	SceneController* sceneController = _currentScene;

	//Touches infinite state machine
	if (Touch.touched())
	{
		//Get touch point and transform due to screen rotation
		TS_Point point = Touch.getPoint();
		swap(point.x,point.y);
		point.y = 240-point.y;

		if (_touched)
		{
			if (point.x != _lastTouchPoint.x || point.y != _lastTouchPoint.y)
			{
				//LOG("Touch Moved");
				//Move event
				sceneController->handleTouchMoved(point,_lastTouchPoint);
			}
		}
		else
		{
			//LOG("Touch down");
			//Touch down event
			sceneController->handleTouchDown(point);
			_touched = true;
		}

		_lastTouchPoint = point;
	}
	else
	{
		if (_touched)
		{
			//LOG("Touch up");
			//Touch up event
			sceneController->handleTouchUp(_lastTouchPoint);
		}

		_touched = false;
	}
}

void ApplicationClass::setup()
{
	//Configure LED pin
	pinMode(LED_PIN, OUTPUT);
	printr.init();

  //Make sure we have a jobs folder
  //TODO: Decide if this is necessary or if the SD card is setup with this path during production
  if (!SD.exists("/jobs"))
  {
      SD.mkdir("/jobs");
  }
}

void ApplicationClass::pingESP()
{
  //Send ping with current version to ESP
  int version = FIRMWARE_BUILDNR;
  _esp->requestTask(TaskID::Ping,sizeof(int),(uint8_t*)&version);
}

void ApplicationClass::resetESP()
{
  pinMode(ESP_RESET, OUTPUT);
  digitalWrite(ESP_RESET, LOW);
  delay(100);
  digitalWrite(ESP_RESET, HIGH);
  pinMode(ESP_RESET, INPUT);
}

void ApplicationClass::loop()
{
  //Peridically send ping to ESP
  if (!_espOK) {
    if ((millis() - _lastESPPing) > 5000) {
      pingESP();
      _lastESPPing = millis();
    }
  }

	//Process Communication with ESP
	_esp->process();

	//run the loop on printr
	printr.loop();

	//Run Animations
	Animator.update();

	StatusLED.loop();

	//Clear the display
	//Display.clear();

  //Handling background jobs
  if (_nextJob != NULL)
  {
    if (_currentJob != NULL) {
      //Send terminating handler
      _currentJob->onWillEnd();
      delete _currentJob;
    }

    _currentJob = _nextJob;
    _nextJob = NULL;

    //Send will start event
    _currentJob->onWillStart();
  }

  if (_currentJob != NULL) {
    _currentJob->loop();
  }

  //UI Handling
	if (_nextScene != NULL)
	{
		//Shut down display to hide the build process of the layout (which is step by step and looks flashy)
		Display.fadeOut();

		//Clear the display
		Display.clear();

		if (_currentScene != NULL)
		{
			delete _currentScene;
		}

		_currentScene = _nextScene;
		_nextScene = NULL;
		_firstSceneLoop = true;
	}

	//Run current controller
	if (_currentScene != NULL)
	{
		//LOG("Run Controller");
		SceneController* sceneController = _currentScene;

		//Call onWillAppear event handler if this is the first time the loop function is called by the scene
		//The default implementation will clear the display!
		if (_firstSceneLoop)
		{
			LOG("First loop");
			Display.clear();

			//Prepare display for this scene (i.e. setting scroll position and scroll offsets, etc)
			sceneController->setupDisplay();

			LOG_VALUE("Appearing scene", sceneController->getName());
			sceneController->onWillAppear();
			LOG("Scene appeared");

/*			Display.fillRect(0,0,50,240,Application.getTheme()->getPrimaryColor());
			Display.setTextRotation(270);
			Display.setCursor(13,130);
			Display.setTextColor(ILI9341_WHITE);
			Display.setFont(PTSansNarrow_24);
			Display.print("PROJECTS");

			Display.setTextRotation(0);*/
		}

		//Touch handling
		handleTouches();

		//Calculate Delta Time
		unsigned long currentTime = millis();
		if (currentTime < _lastTime)
		{
			//millis overflowed, just set delta to 0
			_deltaTime = 0;
		}
		else
		{
			_deltaTime = (float)(currentTime - _lastTime)/1000.0f;
		}

		//Run the scenes loop function
		sceneController->loop();
		_lastTime = millis();

		bool willRefresh = Display.willRefresh();
		if (willRefresh)
		{
			//This should be a good idea as it marks MK20 to be unable to receive data, but this does not work at the moment
			digitalWrite(COMMSTACK_DATAFLOW_PIN,LOW);
		}

		//Relayout screen tiles
		Display.layoutIfNeeded();

		//Update display
		Display.dispatch();

		if (_firstSceneLoop)
		{
			//Set display brightness to full to show what's been built up since we shut down the display
			Display.fadeIn();
		}

		if (willRefresh)
		{
			//This should be a good idea as it marks MK20 to be unable to receive data, but this does not work at the moment
			digitalWrite(COMMSTACK_DATAFLOW_PIN,HIGH);
		}

		_firstSceneLoop = false;
	}

	//Delay for a few ms if no animation is running
	if (!Animator.hasActiveAnimations())
	{
		//delay(16);
	}

}

void ApplicationClass::pushScene(SceneController *scene, bool cancelModal)
{
  if (_currentScene != NULL && _currentScene->isModal() && cancelModal == false)
  {
    //Don't push this scene as the current screen is modal and should not be canceled
    return;
  }

	LOG_VALUE("Pushing scene",scene->getName());

	_nextScene = scene;
}

void ApplicationClass::pushJob(BackgroundJob *job) {
  _nextJob = job;
}

ColorTheme* ApplicationClass::getTheme()
{
	return &_theme;
}


void ApplicationClass::sendScreenshot()
{

}


float ApplicationClass::getDeltaTime()
{
	return _deltaTime;
}


CommStack *ApplicationClass::getESPStack()
{
	return _esp;
}

void ApplicationClass::onCommStackError()
{
	StatusLED.pulse(0.5,false);
}

bool ApplicationClass::runTask(CommHeader &header, const uint8_t *data, size_t dataSize, uint8_t *responseData, uint16_t *responseDataSize, bool* sendResponse, bool* success)
{
	if (_currentScene != NULL && _currentScene->handlesTask(header.getCurrentTask()))
	{
		LOG_VALUE("Current scene handles Task with ID",header.getCurrentTask());
		return _currentScene->runTask(header,data,dataSize,responseData,responseDataSize,sendResponse,success);
	}

  if (_currentJob != NULL && _currentJob->handlesTask(header.getCurrentTask())) {
    return _currentJob->runTask(header,data,dataSize,responseData,responseDataSize,sendResponse,success);
  }}

	LOG_VALUE("Running Task with ID",header.getCurrentTask());
	LOG_VALUE("Comm-Type",header.commType);

	if (header.getCurrentTask() == TaskID::SaveProjectWithID) {
    if (header.commType == Request) {

      StaticJsonBuffer<500> jsonBuffer;
      String jsonObject((const char *) data);
      JsonObject &root = jsonBuffer.parseObject(jsonObject);

      if (root.success()) {
        String url = root["url"];
        if (url.length() > 0) {
          String localFilePath("/projects/");
          localFilePath += root["id"].asString();

          if (SD.exists(localFilePath.c_str())) {
            SD.remove(localFilePath.c_str());
          }

          DownloadFileController *dfc = new DownloadFileController(url, localFilePath);
          Application.pushScene(dfc);
        }
      } else {
        LOG("Could not parse SaveProjectWithID data package from JSON");
      }

      //Do not send a response as we will trigger a "mode" change on ESP in the next request
      *sendResponse = false;
    }
  } else if (header.getCurrentTask() == TaskID::DownloadError) {
    if (header.commType == Request) {

      //Cast data into local error code variable
      uint8_t error = *data;
      DownloadError errorCode = (DownloadError) error;

      if (errorCode == DownloadError::Timeout) {
        Application.pushScene(new ErrorScene("Timeout"));
      } else if (errorCode == DownloadError::InternalServerError) {
        Application.pushScene(new ErrorScene("Internal Server Error"));
      } else if (errorCode == DownloadError::FileNotFound) {
        Application.pushScene(new ErrorScene("File not found"));
      } else if (errorCode == DownloadError::Forbidden) {
        Application.pushScene(new ErrorScene("Forbidden"));
      } else if (errorCode == DownloadError::UnknownError) {
        Application.pushScene(new ErrorScene("Unknown Error"));
      } else if (errorCode == DownloadError::ConnectionFailed) {
        Application.pushScene(new ErrorScene("Connection failed"));
      } else if (errorCode == DownloadError::PrepareDownloadedFileFailed) {
        Application.pushScene(new ErrorScene("File preparation failed"));
      } else if (errorCode == DownloadError::RemoveOldFilesFailed) {
        Application.pushScene(new ErrorScene("Remove old file failed"));
      }

      *sendResponse = false;
    }
  } else if (header.getCurrentTask() == TaskID::FirmwareUpdateError) {
    if (header.commType == Request) {

      //Cast data into local error code variable
      uint8_t error = *data;
      FirmwareUpdateError errorCode = (FirmwareUpdateError) error;

      if (errorCode == FirmwareUpdateError::UnknownError) {
        Application.pushScene(new ErrorScene("Unknown Error"));
      }

      *sendResponse = false;
    }
  } else if (header.getCurrentTask() == TaskID::GetTimeAndDate) {
    if (header.commType == ResponseSuccess) {
      LOG("Loading Date and Time from ESP");
      Display.setCursor(10, 30);
      Display.println("Data available, reading...");

      char datetime[header.contentLength + 1];
      memset(datetime, 0, header.contentLength + 1);
      memcpy(datetime, data, header.contentLength);

      LOG_VALUE("Received Datetime", datetime);

      Display.setCursor(10, 50);
      Display.println("Received datetime from ESP");
      Display.println(datetime);
    }
  } else if (header.getCurrentTask() == TaskID::StartFirmwareUpdate) {
    //We ask ESP therefore we get the response
    if (header.commType == ResponseSuccess) {
      ErrorScene *scene = new ErrorScene("Updating Firmware", false);
      Application.pushScene(scene);
    }
  } else if (header.getCurrentTask() == TaskID::Ping) {
    if (header.commType == ResponseSuccess) {
      //We have received the response from ESP on our ping - do nothing
      int buildNumber = 0;
      memcpy(&buildNumber, data, dataSize);

      //Stop sending pings
      _espOK = true;

      //Communication with ESP established, show project scene
      ProjectsScene *mainScene = new ProjectsScene();
      Application.pushScene(mainScene);
    } else if (header.commType == Request) {
      //Read build number from MK20 firmware
      int buildNumber = 0;
      memcpy(&buildNumber, data, sizeof(int));

      //Stop sending pings to MK20
      _espOK = true;

      //Send ESP build number in response
      buildNumber = FIRMWARE_BUILDNR;
      *sendResponse = true;
      *responseDataSize = sizeof(int);
      memcpy(responseData, &buildNumber, sizeof(int));
    }
  } else if (header.getCurrentTask() == TaskID::ShowFirmwareUpdateNotification) {
    if (header.commType == Request) {
      *sendResponse = false;

      ConfirmFirmwareUpdateScene *scene = new ConfirmFirmwareUpdateScene();
      Application.pushScene(scene);
    }
  } else if (header.getCurrentTask() == TaskID::DebugLog) {
    *sendResponse = false;
  } else if (header.getCurrentTask() == TaskID::RestartESP) {
    *sendResponse = false;

    //Restart ESP
    resetESP();
  } else if (header.getCurrentTask() == TaskID::FirmwareUpdateComplete) {
    //Don't send response as we restart ESP
    *sendResponse = false;

    //Restart ESP
    resetESP();

    //Show Project scene
    ProjectsScene* scene = new ProjectsScene();
    pushScene(scene,true);
  } else if (header.getCurrentTask() == TaskID::FileOpenForWrite) {
    if (header.commType == Request) {

      StaticJsonBuffer<500> jsonBuffer;
      String jsonObject((const char *) data);
      JsonObject &root = jsonBuffer.parseObject(jsonObject);

      if (root.success()) {
        String localFilePath = root["localFilePath"];
        if (localFilePath.length() > 0) {
          if (SD.exists(localFilePath.c_str())) {
            *responseDataSize = 0;
            *sendResponse = true;
            *success = true;

            size_t fileSize = root["fileSize"];

            ReceiveSDCardFile* job = new ReceiveSDCardFile(localFilePath, fileSize);
            Application.pushJob(job);

          } else {
            *responseDataSize = 0;
            *sendResponse = true;
            *success = false;
          }
        }
      } else {
        LOG("Could not parse SaveProjectWithID data package from JSON");
      }

      //Do not send a response as we will trigger a "mode" change on ESP in the next request
      *sendResponse = false;
  }



/*	else if (header.getCurrentTask() == GetJobWithID || header.getCurrentTask() == GetProjectItemWithID)
	{
		if (header.commType == Response)
		{
			//Wait for data to be arrived
			while (!stream->available())
			{
				delay(10);
			}

			//First we ask for the job id which is sent using println on the other side so we read until a newline char
			String jobID = stream->readStringUntil('\n');
			LOG_VALUE("Got Response for GetJobWithID",jobID);

			//Add file suffix to job
			jobID = jobID + ".gcode";

			//Open a file on SD card
			File file = SD.open("job.gcode",O_WRITE);
			if (!file.available())
			{
				//TODO: We should handle that. For now we will have to read data from ESP to clean the pipe but there should be better ways to handle errors
			}

			LOG("File opened for writing. Now waiting for number of bytes to read");

			//Wait for data to be arrived
			while (!stream->available())
			{
				delay(10);
			}

			//We asked for the job and now we got it, the next 4 bytes are the number of bytes sent next
			int32_t numberOfBytes = 0;
			int bytesRead = stream->readBytes((char*)&numberOfBytes,sizeof(int32_t));
			if (bytesRead == sizeof(int32_t))
			{
				LOG_VALUE("Expecting data of size",numberOfBytes);

				//We should have the correct number of bytes to read next
				int32_t currentByteIndex = 0;
				while (currentByteIndex < numberOfBytes)
				{
					if (stream->available())
					{
						int byteRead = stream->read();
						if (byteRead >= 0)
						{
							file.write(byteRead);
						}

						currentByteIndex++;

						if (currentByteIndex % 100 == 0)
						{
							float fraction = (float)currentByteIndex / (float)numberOfBytes;
							DownloadFileController* downloadFileScene = (DownloadFileController*)_currentScene;
							downloadFileScene->getProgressBar()->setValue(fraction);

							LOG_VALUE("Download-Progress",fraction);

							//Update the display - we need to do that here as wo don't run the application loop which does call this method each "frame"
							Display.dispatch();
						}
					}
				}

				LOG("Finished downloading file");
				LOG_VALUE("Download bytes",currentByteIndex);
				LOG_VALUE("Expected number of bytes",numberOfBytes);

				file.close();
			}
		}
	}*/
	return true;
}
