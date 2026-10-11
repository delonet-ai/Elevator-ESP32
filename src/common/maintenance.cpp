#include "maintenance.h"
#include "device_pairing.h"
#include "ota_package.h"
#include "maintenance_protocol.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <mbedtls/platform_util.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
namespace {
#ifdef LIFT_BASE
constexpr uint8_t ROLE=1;
const char *roleName="base";
#ifdef BENCH_COMMANDS
const char *targetMarker="LIFT_TARGET_BENCH_V1";
#else
const char *targetMarker="LIFT_TARGET_BASE_V1";
#endif
#else
constexpr uint8_t ROLE=2;
const char *roleName="remote";
const char *targetMarker="LIFT_TARGET_REMOTE_V1";
#endif
// Only encrypted unicast peers receive credentials. HMAC authenticates the
// application packet too. Responses must match the receiver's fresh challenge.
using Message=MaintenanceMessage;
struct Credentials {char ssid[33],wifiPassword[65],admin[33];};
#ifndef LIFT_BASE
Credentials credentials={};
bool connecting=false;
uint32_t lastProbe=0;
#endif
QueueHandle_t incoming=nullptr;
WebServer server(8080);
String accessPassword,csrf;
bool active=false,serving=false,remoteReady=false,uploadAllowed=false,uploadOK=false;
bool uploadTransaction=false;
uint32_t enteredAt=0,challengeAt=0,rebootAt=0,lastUpload=0;
uint8_t challenge[16]={};
char remoteIp[16]={};
String uploadError;
OtaHeader header={};size_t headerBytes=0,written=0;
mbedtls_sha256_context hash;
bool hashing=false;
void failUpload(const char *why){uploadError=why;uploadAllowed=false;Update.abort();if(hashing){mbedtls_sha256_free(&hash);hashing=false;}}
bool auth(){if(server.authenticate("admin",accessPassword.c_str()))return true;server.requestAuthentication();return false;}
bool mutation(){return auth()&&server.header("X-Lift-Token")==csrf;}
void send(Message &m){pairingTag(&m,sizeof(m)-sizeof(m.tag),m.tag);esp_now_send(pairingMac(),reinterpret_cast<uint8_t*>(&m),sizeof(m));mbedtls_platform_zeroize(&m,sizeof(m));}
void setupServer(){
  const char *headers[]={"X-Lift-Token"};server.collectHeaders(headers,1);
  server.on("/",HTTP_GET,[]{
    if(!auth())return;
    String page=String("<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'><title>Lift OTA</title><h1>Lift ")+roleName+" · OTA</h1><p>Управление отключено. Выберите пакет .liftota для этой платы.</p><input id=f type=file accept=.liftota><button id=b>Обновить</button><progress id=p max=100 value=0></progress><p id=s></p><button id=r>Завершить и перезапустить</button><script>const token='"+csrf+"';b.onclick=()=>{if(!f.files.length)return;b.disabled=true;let x=new XMLHttpRequest();x.open('POST','/update');x.setRequestHeader('X-Lift-Token',token);x.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded/e.total*100};x.onload=()=>{s.textContent=x.responseText;b.disabled=false};x.onerror=()=>{s.textContent='Соединение потеряно. Проверьте состояние платы.';b.disabled=false};let d=new FormData();d.append('firmware',f.files[0]);x.send(d)};r.onclick=async()=>{let x=await fetch('/reboot',{method:'POST',headers:{'X-Lift-Token':token}});s.textContent=await x.text()};</script>";
    server.sendHeader("Cache-Control","no-store");server.send(200,"text/html; charset=utf-8",page);
  });
  server.on("/reboot",HTTP_POST,[]{if(!mutation()){server.send(403,"text/plain","Forbidden");return;}failUpload("Cancelled");server.send(200,"text/plain; charset=utf-8","Перезапуск…");rebootAt=millis()+1000;});
  server.on("/update",HTTP_POST,[]{
    uploadTransaction=false;
    if(!mutation()){failUpload("Unauthorized");server.send(403,"text/plain","Forbidden");return;}
    if(uploadOK&&!Update.end()){uploadOK=false;failUpload("Invalid firmware image");}
    server.send(uploadOK?200:400,"text/plain; charset=utf-8",uploadOK?"Прошивка проверена. Перезапуск…":uploadError);
    if(uploadOK)rebootAt=millis()+1500;
  },[]{
    auto &u=server.upload();lastUpload=millis();
    if(u.status==UPLOAD_FILE_START){
      if(uploadTransaction){uploadOK=false;failUpload("Only one firmware file is allowed");return;}
      uploadTransaction=true;
      failUpload("No valid firmware received");uploadOK=false;headerBytes=written=0;header={};
      uploadAllowed=active&&server.authenticate("admin",accessPassword.c_str())&&server.header("X-Lift-Token")==csrf&&!rebootAt;
      if(!uploadAllowed)return;
    }else if(u.status==UPLOAD_FILE_WRITE&&uploadAllowed){
      size_t offset=0;
      if(headerBytes<sizeof(header)){
        size_t n=std::min(u.currentSize,sizeof(header)-headerBytes);memcpy(reinterpret_cast<uint8_t*>(&header)+headerBytes,u.buf,n);headerBytes+=n;offset=n;
        if(headerBytes==sizeof(header)){
          auto *partition=esp_ota_get_next_update_partition(nullptr);
          if(!partition||!otaHeaderValid(header,ROLE,partition->size)){failUpload("Wrong controller type, size or package format");return;}
          if(!Update.begin(header.size,U_FLASH)){failUpload("Cannot start OTA");return;}
          mbedtls_sha256_init(&hash);mbedtls_sha256_starts_ret(&hash,0);hashing=true;
        }
      }
      size_t n=u.currentSize-offset;
      if(n){if(!hashing||n>header.size-written){failUpload("Invalid image length");return;}
        if(Update.write(u.buf+offset,n)!=n){failUpload("Flash write failed");return;}
        mbedtls_sha256_update_ret(&hash,u.buf+offset,n);written+=n;
      }
    }else if(u.status==UPLOAD_FILE_END&&uploadAllowed){
      if(!hashing||written!=header.size){failUpload("Incomplete image");return;}
      uint8_t digest[32];mbedtls_sha256_finish_ret(&hash,digest);mbedtls_sha256_free(&hash);hashing=false;
      if(memcmp(digest,header.sha256,32)){failUpload("SHA256 mismatch");return;}
      uploadOK=true;uploadAllowed=false;
    }else if(u.status==UPLOAD_FILE_ABORTED){uploadTransaction=false;uploadOK=false;failUpload("Upload interrupted");}
  });
}
void startServer(){if(serving||accessPassword.length()<12)return;csrf=String(esp_random(),HEX)+String(esp_random(),HEX);server.begin();serving=true;Serial.printf("[OTA] http://%s:8080/ (admin, same base password)\n",WiFi.localIP().toString().c_str());}
}
void maintenanceInit(const char *password){
  Serial.printf("[OTA] Target %s\n",targetMarker);
  accessPassword=password;incoming=xQueueCreate(4,sizeof(Message));setupServer();
#ifndef LIFT_BASE
  Preferences p;if(p.begin("lift-net",true)){if(p.getBytesLength("wifi")==sizeof(credentials))p.getBytes("wifi",&credentials,sizeof(credentials));p.end();}
#endif
}
bool maintenanceActive(){return active;}
bool maintenanceReceive(const uint8_t *mac,const uint8_t *data,int len){
  if(len<1||data[0]!=0xD7)return false;
  if(incoming&&pairingReady()&&pairingAccept(mac)&&len==sizeof(Message))xQueueSend(incoming,data,0);
  return true;
}
bool maintenanceStart(){
#ifdef BENCH_COMMANDS
  return false;
#endif
  if(active||WiFi.status()!=WL_CONNECTED)return false;
  active=true;enteredAt=millis();remoteReady=false;remoteIp[0]=0;challengeAt=0;memset(challenge,0,16);startServer();return true;
}
void maintenanceInfo(char *out,size_t size){snprintf(out,size,"{\"active\":%s,\"paired\":%s,\"remoteReady\":%s,\"remoteIp\":\"%s\"}",active?"true":"false",pairingReady()?"true":"false",remoteReady?"true":"false",remoteIp);}
void maintenancePoll(bool idle){
  uint32_t now=millis();Message m;
  for(unsigned i=0;incoming&&i<4&&xQueueReceive(incoming,&m,0)==pdTRUE;++i){
    if(m.version!=1||!pairingVerify(&m,sizeof(m)-sizeof(m.tag),m.tag))continue;
#ifdef LIFT_BASE
    if(m.type==1&&idle&&WiFi.status()==WL_CONNECTED){
      Message reply;reply.type=2;reply.ota=active&&!remoteReady&&now-enteredAt<60000;
      memcpy(reply.nonce,m.nonce,16);
      snprintf(reply.ssid,sizeof(reply.ssid),"%s",WiFi.SSID().c_str());snprintf(reply.wifiPassword,sizeof(reply.wifiPassword),"%s",WiFi.psk().c_str());snprintf(reply.admin,sizeof(reply.admin),"%s",accessPassword.c_str());
      if(reply.ota){memcpy(challenge,m.nonce,16);challengeAt=now;}
      send(reply);
    }else if(m.type==3&&active&&challengeAt&&now-challengeAt<30000&&!memcmp(challenge,m.nonce,16)){
      IPAddress ip;if(m.ip[15]==0&&ip.fromString(m.ip)){remoteReady=true;snprintf(remoteIp,sizeof(remoteIp),"%s",ip.toString().c_str());}
    }
#else
    if(!active&&idle&&maintenanceResponseValid(m,challenge,challengeAt,now)){
      Credentials next={};memcpy(next.ssid,m.ssid,33);memcpy(next.wifiPassword,m.wifiPassword,65);memcpy(next.admin,m.admin,33);
      if(memcmp(&next,&credentials,sizeof(next))){Preferences p;bool saved=false;if(p.begin("lift-net",false)){saved=p.putBytes("wifi",&next,sizeof(next))==sizeof(next);p.end();}if(!saved)continue;credentials=next;}
      challengeAt=0;
      if(m.ota==1){active=true;connecting=true;enteredAt=now;accessPassword=credentials.admin;WiFi.begin(credentials.ssid,credentials.wifiPassword);}
      mbedtls_platform_zeroize(&next,sizeof(next));
    }
#endif
    mbedtls_platform_zeroize(&m,sizeof(m));
  }
#ifndef LIFT_BASE
  if(!active&&idle&&pairingReady()&&now-lastProbe>=1000){lastProbe=now;esp_fill_random(challenge,16);challengeAt=now;Message request;request.type=1;memcpy(request.nonce,challenge,16);send(request);}
  if(active&&WiFi.status()==WL_CONNECTED){
    connecting=false;startServer();
    if(now-lastProbe>=1000){lastProbe=now;Message ack;ack.type=3;memcpy(ack.nonce,challenge,16);snprintf(ack.ip,sizeof(ack.ip),"%s",WiFi.localIP().toString().c_str());send(ack);}
  }
  if(!rebootAt&&active&&((connecting&&now-enteredAt>20000)||(!uploadAllowed&&now-enteredAt>300000)))rebootAt=now+1;
#endif
  if(serving)server.handleClient();
  if(uploadAllowed&&now-lastUpload>15000)failUpload("Upload timeout");
  if(rebootAt&&int32_t(millis()-rebootAt)>=0)ESP.restart();
}
