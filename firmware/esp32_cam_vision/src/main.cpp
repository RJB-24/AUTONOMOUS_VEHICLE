#include <Arduino.h>
#include "esp_camera.h"
#include "TensorFlowLite.h"
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model_data.h"

// AI-Thinker ESP32-CAM pinout. Verify the actual PCB; OV3660 is the sensor, not board model.
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

constexpr int W=96,H=96,CLASSES=4;
const char CODES[CLASSES]={'B','F','L','R'}; // Same alphabetic order as train.py
constexpr size_t ARENA_SIZE=320*1024;
uint8_t *arena;
tflite::MicroInterpreter *interpreter=nullptr;
TfLiteTensor *input=nullptr,*output=nullptr;

bool initCamera(){
  camera_config_t c={}; c.ledc_channel=LEDC_CHANNEL_0; c.ledc_timer=LEDC_TIMER_0;
  c.pin_d0=Y2_GPIO_NUM;c.pin_d1=Y3_GPIO_NUM;c.pin_d2=Y4_GPIO_NUM;c.pin_d3=Y5_GPIO_NUM;
  c.pin_d4=Y6_GPIO_NUM;c.pin_d5=Y7_GPIO_NUM;c.pin_d6=Y8_GPIO_NUM;c.pin_d7=Y9_GPIO_NUM;
  c.pin_xclk=XCLK_GPIO_NUM;c.pin_pclk=PCLK_GPIO_NUM;c.pin_vsync=VSYNC_GPIO_NUM;c.pin_href=HREF_GPIO_NUM;
  c.pin_sscb_sda=SIOD_GPIO_NUM;c.pin_sscb_scl=SIOC_GPIO_NUM;c.pin_pwdn=PWDN_GPIO_NUM;c.pin_reset=RESET_GPIO_NUM;
  c.xclk_freq_hz=20000000;c.pixel_format=PIXFORMAT_RGB565;c.frame_size=FRAMESIZE_QVGA;
  c.jpeg_quality=12;c.fb_count=1;c.fb_location=CAMERA_FB_IN_PSRAM;c.grab_mode=CAMERA_GRAB_WHEN_EMPTY;
  return esp_camera_init(&c)==ESP_OK;
}
void fillInput(camera_fb_t *fb){
  // Centre-crop 240x240 from 320x240 and nearest-neighbour downsample to 96x96.
  for(int y=0;y<H;y++) for(int x=0;x<W;x++){
    int sx=40+(x*240/W), sy=y*240/H, i=(sy*320+sx)*2;
    uint16_t p=(uint16_t(fb->buf[i])<<8)|fb->buf[i+1];
    int o=(y*W+x)*3;
    input->data.uint8[o]=(p>>8)&0xF8;
    input->data.uint8[o+1]=(p>>3)&0xFC;
    input->data.uint8[o+2]=(p<<3)&0xF8;
  }
}
void setup(){
  Serial.begin(9600); // TX0 sends predictions to Nano; disconnect it while uploading.
  if(!psramFound() || g_model_len==0 || !initCamera()){while(true){Serial.println("V,B,0");delay(1000);}}
  static tflite::MicroErrorReporter reporter; static tflite::AllOpsResolver resolver;
  const tflite::Model *model=tflite::GetModel(g_model);
  if(model->version()!=TFLITE_SCHEMA_VERSION){while(true){Serial.println("V,B,0");delay(1000);}}
  arena=(uint8_t*)heap_caps_malloc(ARENA_SIZE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  static tflite::MicroInterpreter staticInterpreter(model,resolver,arena,ARENA_SIZE,&reporter);
  interpreter=&staticInterpreter;
  if(!arena || interpreter->AllocateTensors()!=kTfLiteOk){while(true){Serial.println("V,B,0");delay(1000);}}
  input=interpreter->input(0); output=interpreter->output(0);
}
void loop(){
  camera_fb_t *fb=esp_camera_fb_get();
  if(!fb){Serial.println("V,B,0");delay(100);return;}
  fillInput(fb); esp_camera_fb_return(fb);
  if(interpreter->Invoke()!=kTfLiteOk){Serial.println("V,B,0");delay(100);return;}
  int best=0; for(int i=1;i<CLASSES;i++) if(output->data.uint8[i]>output->data.uint8[best]) best=i;
  float prob=(output->data.uint8[best]-output->params.zero_point)*output->params.scale;
  int pct=constrain((int)lroundf(prob*100),0,100);
  Serial.printf("V,%c,%d\n",CODES[best],pct); delay(80);
}
