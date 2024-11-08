# Make sure to install wireshark in the default location
# And make sure to move USBPCapCMD.exe to /extcap/wireshark/USBPcapCMD.exe in wireshark program files folder

cmd /c '"C:\Program Files\Wireshark\extcap\wireshark\USBPcapCMD.exe"  --extcap-interface \\.\USBPcap1 --extcap-config'

$devadd = Read-Host "Please select device address"

$command = '"C:\Program Files\Wireshark\tshark" -i \\.\USBPcap1 -s 0 -T fields ' +
    '-e usb.transfer_type -e frame.time_epoch -e frame.len -e usb.capdata ' +
    '-e usb.iso.data -e usbvideo.format.index -e usbvideo.frame.index ' +
    '-e usbvideo.frame.width -e usbvideo.frame.height ' +
    '-e usbvideo.streaming.descriptorSubType -e usbvideo.frame.interval ' +
    '-e usbvideo.probe.maxVideoFrameSize -e usbvideo.probe.maxPayloadTransferSize -e usbvideo.format.numFrameDescriptors ' +
    '-E separator=; -Y "usb.device_address == ' + $devadd + '" -Q | .\build\release\oldmanandsea_g.exe'

Write-Output $command

cmd /c $command
