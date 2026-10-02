#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "wifi_config.h"
#define PWDN 32
#define XCLK 0
#define SIOD 26
#define SIOC 27
#define Y9 35
#define Y8 34
#define Y7 39
#define Y6 36
#define Y5 21
#define Y4 19
#define Y3 18
#define Y2 5
#define VSYNC 25
#define HREF 23
#define PCLK 22
WiFiUDP udp; unsigned long lastPacket=0;
static esp_err_t capture(httpd_req_t *req){
  camera_fb_t *fb=esp_camera_fb_get(); if(!fb) return httpd_resp_send_500(req);
  httpd_resp_set_type(req,"image/jpeg");
  httpd_resp_set_hdr(req,"Cache-Control","no-store");
  esp_err_t r=httpd_resp_send(req,(const char*)fb->buf,fb->len); esp_camera_fb_return(fb); return r;
}
void setup(){
  // TX0 forwards commands to Nano SoftwareSerial; 9600 is reliable on ATmega328P.
  Serial.begin(9600);
  camera_config_t c={}; c.ledc_channel=LEDC_CHANNEL_0;c.ledc_timer=LEDC_TIMER_0;
  c.pin_d0=Y2;c.pin_d1=Y3;c.pin_d2=Y4;c.pin_d3=Y5;c.pin_d4=Y6;c.pin_d5=Y7;c.pin_d6=Y8;c.pin_d7=Y9;
  c.pin_xclk=XCLK;c.pin_pclk=PCLK;c.pin_vsync=VSYNC;c.pin_href=HREF;c.pin_sscb_sda=SIOD;c.pin_sscb_scl=SIOC;
  c.pin_pwdn=PWDN;c.pin_reset=-1;c.xclk_freq_hz=20000000;c.pixel_format=PIXFORMAT_JPEG;
  c.frame_size=FRAMESIZE_QVGA;c.jpeg_quality=12;c.fb_count=2;c.grab_mode=CAMERA_GRAB_LATEST;c.fb_location=CAMERA_FB_IN_PSRAM;
  if(esp_camera_init(&c)!=ESP_OK) ESP.restart();
  WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
  while(WiFi.status()!=WL_CONNECTED){delay(250);}
  Serial.print("ESP32_IP,"); Serial.println(WiFi.localIP());
  httpd_config_t cfg=HTTPD_DEFAULT_CONFIG(); httpd_handle_t server=nullptr; httpd_start(&server,&cfg);
  httpd_uri_t uri={.uri="/capture",.method=HTTP_GET,.handler=capture,.user_ctx=nullptr}; httpd_register_uri_handler(server,&uri);
  udp.begin(4210); lastPacket=millis();
}
void loop(){
  int n=udp.parsePacket();
  if(n){
    char b[24]; int got=udp.read(b,sizeof(b)-1); b[got>0?got:0]=0;
    char d=0;int q=-1;
    if(sscanf(b,"V,%c,%d",&d,&q)==2 && strchr("BFLR",d) && q>=0 && q<=100){Serial.printf("V,%c,%d\n",d,q);lastPacket=millis();}
  }
  // Wireless/laptop failure becomes a zero-confidence BLOCKED command.
  static unsigned long lastStop=0;
  if(millis()-lastPacket>700 && millis()-lastStop>250){Serial.println("V,B,0");lastStop=millis();}
  delay(2);
}
