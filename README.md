# Modified eBUS Player
This is the modified version of eBUS Player source code which was originally released and open-sourced by Pleora Technologies (https://www.pleora.com/products/ebus-sdk/).
The player was modified to allow sending the camera data through the network socket to client applications. 
This was needed during my Bachelor thesis (https://github.com/lukasbrchl/BachelorThesis) while recieveing raw temperature data from the infrared FLIR A65 camera.
The original eBUS Player provides access to GenICam registers and the video stream for GigE cameras.

### Prerequisites
* eBUS SDK
* Visual Studio

### Files which differ from original eBUS Player
* ImageSaving.{cpp, h}
* Player.{cpp, h}
* NetworkManager.{cpp, h}

### Usage

* Clone the repository
```
git clone https://github.com/lukasbrchl/Modified-eBUS-Player.git
```
* Build the project using Visual Studio
* Run the player (the network socket is initialized on the player startup) 
* Connect the camera and press the play button.
* After the play button is pressed, the socket is created and the player will wait until the client connects.

### Copyright information
Copyright (c) 2010, Pleora Technologies Inc., All rights reserved. 
