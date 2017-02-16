/*
  This WebServer code is incorporated with very little modification.
  Very simple yet powerful.

  A few new handlers were added at the end, and appropriate server.on
  declarations define them in the Setup code.

  The server supports reading and writing files to/from the SDcard.
  It also serves up HTML files to a browser.  The configuration utility is
  index.htm in the root directory of the SD card.
  The server also came with a great editor utility which, if placed on the SD,
  can be used to edit the web pages or any other text file on the SDcard.
  
  Small parts of the code are imbedded elsewhere as needed in the preamble and Setup sections.
  and "handleClient()" is invoked as often as practical in Loop to keep it running.
  
  The author's copyright and license info follows:

  --------------------------------------------------------------------------------------------------
 
  SDWebServer - Example WebServer with SD Card backend for esp8266

  Copyright (c) 2015  . All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Have a FAT Formatted SD Card connected to the SPI port of the ESP8266
  The web root is the SD Card root folder
  File extensions with more than 3 charecters are not supported by the SD Library
  File Names longer than 8 charecters will be truncated by the SD library, so keep filenames shorter
  index.htm is the default index (works on subfolders as well)

  upload the contents of SdRoot to the root of the SDcard and access the editor by going to http://esp8266sd.local/edit

  ----------------------------------------------------------------------------------------------------------------

*/

void returnOK() {
  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool loadFromSdCard(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SD.open(path.c_str());
  if(dataFile.isDirectory()){
    path += "/index.htm";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }

  if (!dataFile)
    return false;

  if (server.hasArg("download")) dataType = "application/octet-stream";

  if (server.streamFile(dataFile, dataType) != dataFile.size()) {
    msgLog("Server: Sent less data than expected!");
  }

  dataFile.close();
  return true;
}

void handleFileUpload(){
  // if(server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    if(SD.exists((char *)upload.filename.c_str())) SD.remove((char *)upload.filename.c_str());
    uploadFile = SD.open(upload.filename.c_str(), FILE_WRITE);
    DBG_OUTPUT_PORT.print("Upload: START, filename: "); DBG_OUTPUT_PORT.println(upload.filename);
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    DBG_OUTPUT_PORT.print("Upload: WRITE, Bytes: "); DBG_OUTPUT_PORT.println(upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(uploadFile) uploadFile.close();
    DBG_OUTPUT_PORT.print("Upload: END, Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void deleteRecursive(String path){
  File file = SD.open((char *)path.c_str());
  if(!file.isDirectory()){
    file.close();
    SD.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SD.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
}

void handleDelete(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || !SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }
  deleteRecursive(path);
  returnOK();
}

void handleCreate(){
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
      file.write((const char *)0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  returnOK();
}

void printDirectory() {
  if(!server.hasArg("dir")) return returnFail("BAD ARGS");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 dir.close();
}

void handleNotFound(){
  String serverURI = server.uri();
  if(serverURI.startsWith("//")) serverURI.remove(0,1);
  if(serverURI.startsWith("/feed/list")){
    handleGetFeedList();
    return;
  }
  if(serverURI == "/graph/getall"){
    handleGraphGetall();
    return;
  }
  if(serverURI.startsWith("/feed/data")){
    NewService(handleGetFeedData);
    return;
  }
  if(hasSD && loadFromSdCard(server.uri())) return;
  String message = "Server: unsupported request. Method: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += ", URI: ";
  message += server.uri();
  server.send(404, "text/plain", message);
  msgLog(message);
}

/************************************************************************************************
 * 
 * Following handlers added to WebServer for IoTaWatt specific requests
 * 
 **********************************************************************************************/

void handleStatus(){  
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  // WiFiClient client = server.client(); 
  String message = "{";
  boolean firstArg = true;

  if(server.hasArg("stats")){
    if(!firstArg){
      message += ",";
    }
    firstArg = false;
    message += "\"stats\":{\"cyclerate\":" + String(samplesPerCycle,0);
    message += ",\"chanrate\":" + String(cycleSampleRate,1);
    message += ",\"runseconds\":" + String(UnixTime() - programStartTime);
    message += ",\"stack\":" + String(ESP.getFreeHeap());  //system_get_free_heap_size());
    message += "}";
    statServiceInterval = 2;
  }
  
  if(server.hasArg("channels")){
    if(!firstArg){
      message += ",";
    }
    firstArg = false;
    message += "\"channels\":[";
    boolean firstChan = true;
    for(int i=0; i<channels; i++){
      if(channelType[i] == channelTypeUndefined) continue;
      if(!firstChan){
        message += ","; 
      }
      firstChan = false;
      message += "{\"channel\":" + String(i);

      if(channelType[i] == channelTypeVoltage){
        message += ",\"Vrms\":" + String(statBuckets[i].volts,1);
        message += ",\"Hz\":" + String(statBuckets[i].hz,1);
      }
      else if(channelType[i] == channelTypePower){
        message += ",\"Watts\":" + String(statBuckets[i].watts,0);
        message += ",\"Irms\":" + String(statBuckets[i].amps,3);
        if(statBuckets[i].watts > 10){
          message += ",\"Pf\":" + String(statBuckets[i].watts/(statBuckets[i].amps*statBuckets[Vchannel[i]].volts),2);
        }       
      }
      message += "}";
    }
    message += "]";
  }

  if(server.hasArg("voltage")){
    if(!firstArg){
      message += ",";
    }
    firstArg = false;
    int Vchan = server.arg("channel").toInt();
    message += "\"voltage\":" + String(buckets[Vchan].volts,1);
  }
  
  message += "}";
  server.sendContent(message);
}

void handleVcal(){
  if(!server.hasArg("channel") || !server.hasArg("cal")){
    server.send(200, "text/json", "Missing parameters");
    return;
  }
  int channel = server.arg("channel").toInt();
  calibration[channel] = server.arg("cal").toInt();
  float Vrms = sampleVoltage(channel, calibration[channel]);
  String json = "{\"vrms\":" + String(Vrms,1) + "}";  
  server.send(200, "text/json", json);  
}

void handleCommand(){

  if(server.hasArg("restart")) {
    server.send(200, "text/plain", "ok");
    msgLog("Restart command received.");
    delay(500);
    ESP.restart();
  } 
  if(server.hasArg("calibrate")){
    msgLog("calibrate:", server.arg("calibrate").toInt());
    msgLog(", ref:", server.arg("ref").toInt());
    server.send(200, "text/plain", "ok");
    calibrationVchan = server.arg("calibrate").toInt();
    calibrationRefChan = server.arg("ref").toInt();
    calibrationMode = true;
  }
}

void handleGetFeedList(){
  
  String reply = "[";
  for(int i=0; i<channels; i++){
    if(channelType[i] != channelTypeUndefined){
      if(channelType[i] == channelTypeVoltage){
        addFeed("voltage",QUERY_VOLTAGE,reply,i);
        addFeed("Frequency",QUERY_FREQUENCY,reply,i);
      }
      else if(channelType[i] == channelTypePower){
        addFeed("Power",QUERY_POWER,reply,i);
        addFeed("Energy",QUERY_ENERGY,reply,i);
        addFeed("Power Factor",QUERY_PF,reply,i);
      }
    }
  }
  reply.remove(reply.length()-3);
  reply += "]\r\n";
  server.send(200, "application/json", reply);
  Serial.println(reply);
}

void addFeed(char* tag, int code, String &reply, int i){
  reply += "{\"id\":\"" + String(i*10+code) + "\",\"tag\":\"" + tag + "\",\"name\":";
  if(channelName[i] == "") {
    reply += "\"chan" + String(i) + "\"";   
  }
  else {
    reply += "\"" + channelName[i] + "\"";
  }
  reply += "},\r\n";
}

void handleGraphGetall(){
  return;
  server.send(200, "ok", "{}");
}

