
This is the command line tool to upload new firmware images to the esp.

use the script as following 
python3 iwad-ota.py -d -i LOCAL_IP_OF_ESP -f PAHT_TO_COMPILED_CODE.bin -a PASSWORD
I.e
python3 iwad-ota.py -d -i 192.168.12.76 -f Blinkiblink.ino.feather_esp32.bin -a SuperSecret 
