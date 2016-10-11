//
// Created by Phillip Schuster on 11.10.16.
//

#ifndef ESP_PUSHFILETOSDCARD_H
#define ESP_PUSHFILETOSDCARD_H

#include "core/Mode.h"

class PushFileToSDCard: public Mode
{
public:
    PushFileToSDCard(const String &localFilePath, const String &targetFilePath, bool showUI=false);
    ~PushFileToSDCard();

    void loop();
    void onWillStart();
    void onWillEnd();

    //bool handlesTask(TaskID taskID);
    bool runTask(CommHeader& header, const uint8_t* data, size_t dataSize, uint8_t* responseData, uint16_t* responseDataSize, bool* sendResponse, bool* success);
    virtual bool handlesTask(TaskID taskID);
    String getName();

private:
    String _localFilePath;
    String _targetFilePath;
    bool _waitForResponse;
    bool _fileOpen;
    File _localFile;
    size_t _bytesLeft;
};

#endif //ESP_PUSHFILETOSDCARD_H
