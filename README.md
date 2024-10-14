# uvc_frame_detector

version 0.0.2
included linux and window verison  
each using usbmon and tshark for stream  

## Usage


### In Window  
(only cmd works, not ps1, open in dev mode if possible)
### Build
0. mkdir log
1. mkdir build
2. cd build
3. cmake ..
4. cmake --build .
5. cd Debug

### Run  
To run this code in window, must install wireshark in computer with USBPcap  
If USBPcap is not installed with Wireshark, reinstall wireshark and check USBPcap install  
After install, reboot  
C:\Program Files\Wireshark
Find Wireshark.exe > go to Help(H) > Wireshark info(A) > Folder  
Check if Extcap Path is set extcap/wireshark  
If so go to C:\Program Files\Wireshark\extcap and move USBPcapCMD.exe to wireshark directory  
  
Go to project Directory  
0. cd build  
For BULK  
"C:\Program Files\Wireshark\tshark" -i \\.\USBPcap1 -T fields -e usb.transfer_type -e frame.time_epoch -e frame.len -e usb.capdata -E separator=; -l | C:\-----PROJECT_DIRECTORY_PATH-----\uvc_frame_detector\build\Debug\oldmanandsea.exe -fw 1280 -fh 720 -fps 30 -ff mjpeg -mf 16777216

For ISO
"C:\Program Files\Wireshark\tshark" -i \\.\USBPcap1 -T fields -e usb.transfer_type -e frame.time_epoch -e frame.len -e usb.iso.data -E separator=; -l | C:\-----PROJECT_DIRECTORY_PATH-----\uvc_frame_detector\build\Debug\oldmanandsea.exe -fw 1280 -fh 720 -fps 30 -ff mjpeg -mf 16777216

Can find maximum frame size and maximum payload size in  
for Window > download usb device tree viewer and check video streaming format type descriptor: dwMaxVideoFrameBufferSize
for linux type lsusb -v and find dwMaxVideoFrameBufferSize  
if leave blank for -fw -fh -fps -ff -mf -mp, 1024 720 30 mjpg 16777216 and 1310720 will be set automatically  
each indicate frame_width frame_height frame_per_sec frame_format max_frame_size max_payload_size  

-e usb.transfer_type -e frame.time_epoch -e frame.len -e usb.iso.data // Must be in correct order  
if you are in build directory, can change C:\-----PROJECT_DIRECTORY_PATH-----\build into .\Debug\oldmanandsea.exe  



### In LINUX

### Build
0. sudo modprobe usbmon
1. mkdir log
2. mkdir build
3. cd build
4. cmake ..
5. make

### Run 
0. cd build
1. lsusb <br/>
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub <br/>
Bus 001 Device 002: ID 80ee:0021 VirtualBox USB Tablet <br/>
Bus 001 Device 004: ID 2e1a:4c01 Insta360 Insta360 Link <br/>
Bus 001 Device 005: ID 046d:085e Logitech, Inc. BRIO Ultra HD Webcam <br/>
Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub <br/>
2. sudo ./moncapler -in usbmon1 -bs 41536 -bn 1 -dn 4 -fw 1280 -fh 720 -fps 30 -ff mjpeg -mf -mp -v 2 -lv 1 <br/>

-interface <br/>
usbmon0 for all usb transfers, usbmon1 only for usb bus 1, usbmon2 only for usb bus 2 ...<br/>
-bus number, device number <br/>
can find by lsusb <br/>
-frame width, frame height, frame per second, frame format <br/>
needs to be designated by user <br/>
some of the tests will not be played <br/>
currently auto set to 1280x720 <br/>
-verbose, verbose log<br/>
setting up levels of printings in screen and log 

3. run any camera appliation, guvcview, cheese, vlc, opencv ... e.g.) guvcview

### See Usage
0. sudo ./moncapler

### Example without camera
0. ./example
To test validation algorithm, can test with colour bar screen of jpeg<br/>
Build with cmake above<br/>

### Test Codes
0. ./valid_test
Build with cmake above<br/>


## How It Works

It brings the data of the usbmon (usbmonitor)<br/>
So you will have to choose which usbmon to use<br/>
And then it filters the data by looking at urb header and find specific device's urb<br/>
When it is found, devide them into IN OUT, CONTROL BULK ISO<br/>
And recombine urb block with each algorithms to have complete transfer data,<br/>
starting with payload header<br/>
Then another thread validate the transferred data by looking at the headers<br/>
When validation is finished, transfers are combined into a frame<br/>
When it is done, fps are calculated and shows whether frame has errors<br/>
Error statistics will be given<br/>
Below is one of them <br/>

### Example Result

### For window & linux

press ctrl + c once to see statistics  

Payload Error Statistics:  
No Error: 133 (100%)  
Empty Payload: 0 (0%)  
Max Payload Overflow: 0 (0%)  
Error Bit Set: 0 (0%)  
Length Out of Range: 0 (0%)  
Length Invalid: 0 (0%)  
Reserved Bit Set: 0 (0%)  
End of Header Bit: 0 (0%)  
Toggle Bit Overlapped: 0 (0%)  
Frame Identifier Mismatch: 0 (0%)    
Swap: 0 (0%)  
Missing EOF: 0 (0%)  
Unknown Error: 0 (0%)  
  
Frame Error Statistics:  
No Error: 133 (79.6407%)  
Frame Drop: 34 (20.3593%)  
Frame Error: 0 (0%)  
Max Frame Overflow: 0 (0%)  
Invalid YUYV Raw Size: 0 (0%)  
Same Different PTS: 0 (0%)  


### For Linux only

Capture Statistics:<br/>
  
Total Packets received in usbmon: 6982<br/>
Total Packets dropped by system buffer: 10122<br/>
Total Packets dropped by interface: 0<br/>
  
    
Total Packets counted in usbmon by application: 6952<br/>
Total Packet Length bytes: 40249996<br/>
Total Captured Packet Length bytes: 40249996<br/>
  
  
Filtered Packets (busnum=1, devnum=4): 6940<br/>
Filtered Packet Length bytes (busnum=1, devnum=4): 40249115<br/>
Filtered Captured Packet Length bytes (busnum=1, devnum=4): 40249115<br/>
  
  
If Filtered Packet Length bytes and Filtered Captured Packet Length bytes are different, then test enviroment is maybe unstable <br/>
Starting camera after running code may cause difference in this value, ignore it if think unnecessary <br/>
  
### Saved log file
To compare raw data with result, go to log dir<br/>

frames_log.txt can see every frame packets information  

### Test Codes
For now  
frame_test_bulk,  
frame_test_iso,  
valid_test  
is available  
all tests passed 100  

test_packaet_handler is also available for linux  




# uvcperf

## Usage
Same building method written above  
Changed libuvc streaming part and made valiate statistics for streaming  
ONLY ON LINUX environment  

## RUN
0. cd build
1. sudo ./uvcperf  
ctrl + c to end  




### TODO
make that with graph togeth <br/>
make jpeg data into .jpeg file to sea the err frame <br/>
