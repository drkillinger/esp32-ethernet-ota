#ifndef LWIP_OPEN_SRC
#define LWIP_OPEN_SRC
#endif
#include <functional>
#include <Ethernet.h>
//#include <WiFiUdp.h>
#include "EthernetOTA.h"
//#include "ESPmDNS.h" //ESP is broken, has hardcoded depends on Wifi
#include <EthernetBonjour.h> // https://github.com/TrippyLighting/EthernetBonjour
#include "MD5Builder.h"
#include "Update.h"


// #define OTA_DEBUG Serial

EthernetOTAClass::EthernetOTAClass()
: _port(0)
, _initialized(false)
, _rebootOnSuccess(true)
, _mdnsEnabled(true)
, _state(OTA_IDLE)
, _size(0)
, _cmd(0)
, _ota_port(0)
, _ota_timeout(1000)
, _start_callback(NULL)
, _end_callback(NULL)
, _error_callback(NULL)
, _progress_callback(NULL)
{
}

EthernetOTAClass::~EthernetOTAClass(){
    _udp_ota.stop();
}

EthernetOTAClass& EthernetOTAClass::onStart(THandlerFunction fn) {
    _start_callback = fn;
    return *this;
}

EthernetOTAClass& EthernetOTAClass::onEnd(THandlerFunction fn) {
    _end_callback = fn;
    return *this;
}

EthernetOTAClass& EthernetOTAClass::onProgress(THandlerFunction_Progress fn) {
    _progress_callback = fn;
    return *this;
}

EthernetOTAClass& EthernetOTAClass::onError(THandlerFunction_Error fn) {
    _error_callback = fn;
    return *this;
}

EthernetOTAClass& EthernetOTAClass::setPort(uint16_t port) {
    if (!_initialized && !_port && port) {
        _port = port;
    }
    return *this;
}

EthernetOTAClass& EthernetOTAClass::setHostname(const char * hostname) {
    if (!_initialized && !_hostname.length() && hostname) {
        _hostname = hostname;
    }
    return *this;
}

String EthernetOTAClass::getHostname() {
    return _hostname;
}

EthernetOTAClass& EthernetOTAClass::setPassword(const char * password) {
    if (!_initialized && !_password.length() && password) {
        MD5Builder passmd5;
        passmd5.begin();
        passmd5.add(password);
        passmd5.calculate();
        _password = passmd5.toString();
    }
    return *this;
}

EthernetOTAClass& EthernetOTAClass::setPasswordHash(const char * password) {
    if (!_initialized && !_password.length() && password) {
        _password = password;
    }
    return *this;
}

EthernetOTAClass& EthernetOTAClass::setPartitionLabel(const char * partition_label) {
    if (!_initialized && !_partition_label.length() && partition_label) {
        _partition_label = partition_label;
    }
    return *this;
}

String EthernetOTAClass::getPartitionLabel() {
    return _partition_label;
}

EthernetOTAClass& EthernetOTAClass::setRebootOnSuccess(bool reboot){
    _rebootOnSuccess = reboot;
    return *this;
}

EthernetOTAClass& EthernetOTAClass::setMdnsEnabled(bool enabled){
    _mdnsEnabled = enabled;
    return *this;
}

bool EthernetOTAClass::begin() {
    if (_initialized){
        log_w("already initialized");
        return false;
    }

    if (!_port) {
        _port = 3232;
    }

    if(!_udp_ota.begin(_port)){
        log_e("udp bind failed");
        return false;
    }


    if (!_hostname.length()) {
        char tmp[20];
        uint8_t mac[6];
        Ethernet.MACAddress(mac);
        sprintf(tmp, "esp32-%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        _hostname = tmp;
    }
    if(_mdnsEnabled){
      Serial.print("mDNS starting with: ");
      Serial.println(_hostname.c_str());

      if( EthernetBonjour.begin("arduino")) {
      //if (MDNS.begin(_hostname.c_str() ) ) {
        Serial.println("MDNS successful.");
      } else {
        Serial.println("MDNS failed.");
      }
      EthernetBonjour.addServiceRecord(_hostname.c_str(),
                                   _port,
                                   MDNSServiceTCP);
      //MDNS.enableArduino(_port, (_password.length() > 0));
    }
    _initialized = true;
    _state = OTA_IDLE;
    log_i("OTA server at: %s.local:%u", _hostname.c_str(), _port);
    return true;
}

int EthernetOTAClass::parseInt(){
    char data[INT_BUFFER_SIZE];
    uint8_t index = 0;
    char value;
    while(_udp_ota.peek() == ' ') _udp_ota.read();
    while(index < INT_BUFFER_SIZE - 1){
        value = _udp_ota.peek();
        if(value < '0' || value > '9'){
            data[index++] = '\0';
            return atoi(data);
        }
        data[index++] = _udp_ota.read();
    }
    return 0;
}

String EthernetOTAClass::readStringUntil(char end){
    String res = "";
    int value;
    while(true){
        value = _udp_ota.read();
        if(value <= 0 || value == end){
            return res;
        }
        res += (char)value;
    }
    return res;
}

void EthernetOTAClass::_onRx(){
    if (_state == OTA_IDLE) {
        int cmd = parseInt();
        if (cmd != U_FLASH && cmd != U_SPIFFS)
            return;
        _cmd  = cmd;
        _ota_port = parseInt();
        _size = parseInt();
        _udp_ota.read();
        _md5 = readStringUntil('\n');
        _md5.trim();
        if(_md5.length() != 32){
            log_e("bad md5 length");
            return;
        }

        if (_password.length()){
            MD5Builder nonce_md5;
            nonce_md5.begin();
            nonce_md5.add(String(micros()));
            nonce_md5.calculate();
            _nonce = nonce_md5.toString();

            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.printf("AUTH %s", _nonce.c_str());
            _udp_ota.endPacket();
            _state = OTA_WAITAUTH;
            return;
        } else {
            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.print("OK");
            _udp_ota.endPacket();
            _ota_ip = _udp_ota.remoteIP();
            _state = OTA_RUNUPDATE;
        }
    } else if (_state == OTA_WAITAUTH) {
        int cmd = parseInt();
        if (cmd != U_AUTH) {
            log_e("%d was expected. got %d instead", U_AUTH, cmd);
            _state = OTA_IDLE;
            return;
        }
        _udp_ota.read();
        String cnonce = readStringUntil(' ');
        String response = readStringUntil('\n');
        if (cnonce.length() != 32 || response.length() != 32) {
            log_e("auth param fail");
            _state = OTA_IDLE;
            return;
        }

        String challenge = _password + ":" + String(_nonce) + ":" + cnonce;
        MD5Builder _challengemd5;
        _challengemd5.begin();
        _challengemd5.add(challenge);
        _challengemd5.calculate();
        String result = _challengemd5.toString();

        if(result.equals(response)){
            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.print("OK");
            _udp_ota.endPacket();
            _ota_ip = _udp_ota.remoteIP();
            _state = OTA_RUNUPDATE;
        } else {
            _udp_ota.beginPacket(_udp_ota.remoteIP(), _udp_ota.remotePort());
            _udp_ota.print("Authentication Failed");
            log_w("Authentication Failed");
            _udp_ota.endPacket();
            if (_error_callback) _error_callback(OTA_AUTH_ERROR);
            _state = OTA_IDLE;
        }
    }
}

void EthernetOTAClass::_runUpdate() {
    const char *partition_label = _partition_label.length() ? _partition_label.c_str() : NULL;
    if (!Update.begin(_size, _cmd, -1, LOW, partition_label)) {

        log_e("Begin ERROR: %s", Update.errorString());

        if (_error_callback) {
            _error_callback(OTA_BEGIN_ERROR);
        }
        _state = OTA_IDLE;
        return;
    }
    Update.setMD5(_md5.c_str());

    if (_start_callback) {
        _start_callback();
    }
    if (_progress_callback) {
        _progress_callback(0, _size);
    }

    //WiFiClient client;
    EthernetClient client;
    if (!client.connect(_ota_ip, _ota_port)) {
        if (_error_callback) {
            _error_callback(OTA_CONNECT_ERROR);
        }
        _state = OTA_IDLE;
    }

    uint32_t written = 0, total = 0, tried = 0;

    while (!Update.isFinished() && client.connected()) {
        size_t waited = _ota_timeout;
        size_t available = client.available();
        while (!available && waited){
            delay(1);
            waited -=1 ;
            available = client.available();
        }
        if (!waited){
            if(written && tried++ < 3){
                log_i("Try[%u]: %u", tried, written);
                if(!client.printf("%u", written)){
                    log_e("failed to respond");
                    _state = OTA_IDLE;
                    break;
                }
                continue;
            }
            log_e("Receive Failed");
            if (_error_callback) {
                _error_callback(OTA_RECEIVE_ERROR);
            }
            _state = OTA_IDLE;
            Update.abort();
            return;
        }
        if(!available){
            log_e("No Data: %u", waited);
            _state = OTA_IDLE;
            break;
        }
        tried = 0;
        static uint8_t buf[1460];
        if(available > 1460){
            available = 1460;
        }
        size_t r = client.read(buf, available);
        if(r != available){
            log_w("didn't read enough! %u != %u", r, available);
        }

        written = Update.write(buf, r);
        if (written > 0) {
            if(written != r){
                log_w("didn't write enough! %u != %u", written, r);
            }
            if(!client.printf("%u", written)){
                log_w("failed to respond");
            }
            total += written;
            if(_progress_callback) {
                _progress_callback(total, _size);
            }
        } else {
            log_e("Write ERROR: %s", Update.errorString());
        }
    }

    if (Update.end()) {
        client.print("OK");
        client.stop();
        delay(10);
        if (_end_callback) {
            _end_callback();
        }
        if(_rebootOnSuccess){
            //let serial/network finish tasks that might be given in _end_callback
            delay(100);
            ESP.restart();
        }
    } else {
        if (_error_callback) {
            _error_callback(OTA_END_ERROR);
        }
        Update.printError(client);
        client.stop();
        delay(10);
        log_e("Update ERROR: %s", Update.errorString());
        _state = OTA_IDLE;
    }
}

void EthernetOTAClass::end() {
    _initialized = false;
    _udp_ota.stop();
    if(_mdnsEnabled){
       //MDNS.end();
       //EthernetBonjour has no end()
       Serial.println("EthernetOTAClass::end() but cannot EthernetBonjour.end()");
       
    }
    _state = OTA_IDLE;
    log_i("OTA server stopped.");
}

void EthernetOTAClass::handle() {
    if (!_initialized) {
        return;
    }

    if (_state == OTA_RUNUPDATE) {
        _runUpdate();
        _state = OTA_IDLE;
    }
    if(_udp_ota.parsePacket()){
        _onRx();
    }
    _udp_ota.flush(); // always flush, even zero length packets must be flushed.

    if(_mdnsEnabled){ 
      //MDNS.update();
      EthernetBonjour.run();
    }
}

int EthernetOTAClass::getCommand() {
    return _cmd;
}

void EthernetOTAClass::setTimeout(int timeoutInMillis) {
    _ota_timeout = timeoutInMillis;
}

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_ETHERNETOTA)
EthernetOTAClass EthernetOTA;
#endif
