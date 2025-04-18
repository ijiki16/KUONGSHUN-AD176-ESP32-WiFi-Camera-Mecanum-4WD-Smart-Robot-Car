
#include <esp32-hal-ledc.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"

extern int gpLed;
byte txdata[4] = {0xAB, 0, 0, 0xFF};//{0xAB, 25, 0, 0xFF};{0xAB, 29, 0, 0xFF};{0xAB, 30, 0, 0xFF};
const int Forward       = 163;                               // 前进
const int Backward      = 92;                              // 后退
const int Turn_Left     = 106;                              // 左平移
const int Turn_Right    = 149;                              // 右平移
const int Top_Left      = 34;                               // 左上移动
const int Bottom_Left   = 72;                              // 左下移动
const int Top_Right     = 129;                               // 右上移动
const int Bottom_Right  = 20;                               // 右下移动
const int Stop          = 0;                                // 停止
const int Contrarotate  = 83;                              // 逆时针旋转
const int Clockwise     = 172;                               // 顺时针旋转
const int Model1        = 25;                               // 模式1
const int Model2        = 26;                               // 模式2
const int Model3        = 27;                               // 模式3
const int Model4        = 28;                               // 模式4
const int Speed         = 29;                               // 速度
const int Servo         = 30;                               // 舵机
const int MotorLeft     = 230;                              // 舵机左转
const int MotorRight    = 231;                              // 舵机右转

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    size_t out_len, out_width, out_height;
    uint8_t * out_buf;
    bool s;
    {
        size_t fb_len = 0;
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 80, jpg_encode_stream, &jchunk)?ESP_OK:ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
        esp_camera_fb_return(fb);
        int64_t fr_end = esp_timer_get_time();
        Serial.printf("JPG: %uB %ums\n", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
        return res;
    }

    bool image_matrix = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
    if (!image_matrix) {
        esp_camera_fb_return(fb);
        Serial.println("dl_matrix3du_alloc failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    
    out_len = fb->width * fb->height * 3;
    out_width = fb->width;
    out_height = fb->height;
    out_buf = (uint8_t*)malloc(out_len);

    s = fmt2rgb888(fb->buf, fb->len, fb->format, out_buf);
    esp_camera_fb_return(fb);
    if(!s){
        free(out_buf);
        Serial.println("to rgb888 failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    jpg_chunking_t jchunk = {req, 0};
    s = fmt2jpg_cb(out_buf, out_len, out_width, out_height, PIXFORMAT_RGB888, 90, jpg_encode_stream, &jchunk);
    free(out_buf);
    if(!s){
        Serial.println("JPEG compression failed");
        return ESP_FAIL;
    }

    int64_t fr_end = esp_timer_get_time();
    return res;
}

static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
             {
                if(fb->format != PIXFORMAT_JPEG){
                    bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                    esp_camera_fb_return(fb);
                    fb = NULL;
                    if(!jpeg_converted){
                        Serial.println("JPEG compression failed");
                        res = ESP_FAIL;
                    }
                } else {
                    _jpg_buf_len = fb->len;
                    _jpg_buf = fb->buf;
                }
            }
        }
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
        /*int64_t fr_end = esp_timer_get_time();
        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
        Serial.printf("MJPG: %uB %ums (%.1ffps)\r\n",
            (uint32_t)(_jpg_buf_len),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time           
        );*/
    }

    last_frame = 0;
    return res;
}

enum state {fwd,rev,stp};
state actstate = stp;

static esp_err_t cmd_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;
    //Serial.println(variable);
    if(!strcmp(variable, "framesize")) 
    {
        Serial.println("framesize");
        if(s->pixformat == PIXFORMAT_JPEG) res = s->set_framesize(s, (framesize_t)val);
    }
    else if(!strcmp(variable, "quality")) 
    {
      Serial.println("quality");
      res = s->set_quality(s, val);
    }
    //Remote Control Car 
    //Don't use channel 1 and channel 2
    else if(!strcmp(variable, "flash")) 
    {
      ledcWrite(7,val);
    }  
    else if(!strcmp(variable, "speed")) 
    {
      if      (val > 255) val = 255;
      else if (val <   0) val = 0;       
      txdata[1] = Speed;
      txdata[2] = val;
      Serial.write(txdata, 4);
    }           
    else if(!strcmp(variable, "servo"))
    {
      if      (val > 180) val = 180;
      else if (val < 0) val = 0;     
      //ledcWrite(8,10*val);
      txdata[1] = Servo;
      txdata[2] = val;
      Serial.write(txdata, 4);
    }
    else if(!strcmp(variable, "model")) 
    {
      txdata[2] = 0;
      if (val==2)
      {
        txdata[1] = Model2;
        Serial.write(txdata, 4);
      }
      if (val==3)
      {
        txdata[1] = Model3;
        Serial.write(txdata, 4);
      }
      if (val==4)
      {
        txdata[1] = Model4;
        Serial.write(txdata, 4);
      }
    }
    else if(!strcmp(variable, "car")) 
    {  
      txdata[1] = Model1;
      if (val==1) 
      {
        txdata[2] = Forward;
        Serial.write(txdata, 4);
        //Serial.println("Go");
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);
      }
      else if (val==2) 
      {   
        txdata[2] = Turn_Right;
        Serial.write(txdata, 4);
        //Serial.println("Right");
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);
      }
      else if (val==3) 
      {
        txdata[2] = Stop;
        Serial.write(txdata, 4);
        //Serial.println("Stop");
        actstate = stp;       
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2); 
      }
      else if (val==4) 
      {
        txdata[2] = Turn_Left;
        Serial.write(txdata, 4);
        //Serial.println("Left");
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);        
      }
      else if (val==5) 
      {
        txdata[2] = Backward;
        Serial.write(txdata, 4);
        //Serial.println("Back");  
        actstate = rev;      
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==6) 
      {
        txdata[2] = Top_Left;
        Serial.write(txdata, 4);
        //Serial.println("Back");       
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==7) 
      {
        txdata[2] = Top_Right;
        Serial.write(txdata, 4);
        //Serial.println("Back");      
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==8) 
      {
        txdata[2] = Bottom_Left;
        Serial.write(txdata, 4);
        //Serial.println("Back");      
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==9) 
      {
        txdata[2] = Bottom_Right;
        Serial.write(txdata, 4);
        //Serial.println("Back");       
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==10) 
      {
        txdata[2] = Clockwise;
        Serial.write(txdata, 4);
        //Serial.println("Back");      
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==11) 
      {
        txdata[2] = Model1;
        Serial.write(txdata, 4);
        //Serial.println("Back");       
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==12) 
      {
        txdata[2] = Model2;
        Serial.write(txdata, 4);
        //Serial.println("Back");     
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==13) 
      {
        txdata[2] = Model3;
        Serial.write(txdata, 4);
        //Serial.println("Back");      
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==14) 
      {
        txdata[2] = Model4;
        Serial.write(txdata, 4);
        //Serial.println("Back");      
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
      else if (val==15) 
      {
        txdata[2] = Contrarotate;
        Serial.write(txdata, 4);
        //Serial.println("Back");      
        //httpd_resp_set_type(req, "text/html");
        //return httpd_resp_send(req, "OK", 2);              
      }
    }        
    else 
    { 
      Serial.println("variable");
      res = -1; 
    }

    if(res){ return httpd_resp_send_500(req); }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';

    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html>
<html>
    <head>
        <meta charset="utf-8">
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <title>KUONGSHUN ESP32CAM ROBOT</title>
        <style>
            *{
                padding: 0; margin: 0;
                font-family:monospace;
            }

            *{  
                -webkit-touch-callout:none;  
                -webkit-user-select:none;  
                -khtml-user-select:none;  
                -moz-user-select:none;  
                -ms-user-select:none;  
                user-select:none;  
            }

        canvas {
        margin: auto;
        display: block;

        }
        .tITULO{
            text-align: center;
            color: rgb(97, 97, 97);
            
        }
        .LINK{
            color: red;
            width: 60px;
            margin: auto;
            display: block;
            font-size: 14px;
        }
        .cont_stream{
            width: 90%;
            max-width: 700px;

            border: 1px solid red;
            margin: auto;
            display:block;
        }
        .cont_flex{
            margin: 20px auto 20px;
            width: 90%;
            max-width: 400px;
            display: flex;
            flex-wrap: wrap;
            justify-content: space-around;
        }
        .cont_flex_threebuttom{
            margin: 20px auto 20px;
            width: 70%;
            max-width: 400px;
            display: flex;
            flex-wrap: wrap;
            justify-content: space-around;
        }
        .cont_flex_stream{
            margin: 20px auto 20px;
            width: 70%;
            max-width: 400px;
            display: flex;
            flex-wrap: wrap;
            justify-content: space-around;
        }
        .cont_flex_model{
            margin: 20px auto 20px;
            width: 78%;
            max-width: 400px;
            display: flex;
            flex-wrap: wrap;
            justify-content: space-around;
        }
        .cont_flex_twobuttom{
            margin: 20px auto 20px;
            width: 93%;
            max-width: 400px;
            display: flex;
            flex-wrap: wrap;
            justify-content: space-around;
        }
        .cont_flex button{
            width: 70px;
            height: 30px;
            border: none;
            background-color: red;
            border-radius: 10px;
            color: white;
        }
        .cont_flex_stream button{
            width: 70px;
            height: 30px;
            border: none;
            background-color: green;
            border-radius: 10px;
            color: white;
        }
        .cont_flex_model button{
            width: 100px;
            height: 30px;
            border: none;
            background-color: yellow;
            border-radius: 10px;
            color: black;
        }
        .cont_flex_threebuttom button{
            width: 70px;
            height: 30px;
            border: none;
            background-color: red;
            border-radius: 10px;
            color: white;
        }
        .cont_flex_twobuttom button{
            width: 70px;
            height: 30px;
            border: none;
            background-color: red;
            border-radius: 10px;
            color: white;
        }
        .cont_flex button:active{
            background-color: blue;
        }
        .cont_flex_twobuttom button:active{
            background-color: blue;
        }
        .cont_flex_threebuttom button:active{
            background-color: blue;
        }
        .cont_flex_model button:active{
            background-color: blue;
        }

        input{-webkit-user-select:auto;} 
        input[type=range]{-webkit-appearance:none;width:300px;height:25px;background:#cecece;cursor:pointer;margin:0}
        input[type=range]:focus{outline:0}
        input[type=range]::-webkit-slider-runnable-track{width:100%;height:2px;cursor:pointer;background:#EFEFEF;border-radius:0;border:0 solid #EFEFEF}
        input[type=range]::-webkit-slider-thumb{border:1px solid rgba(0,0,30,0);height:22px;width:22px;border-radius:50px;background:#ff3034;cursor:pointer;-webkit-appearance:none;margin-top:-10px}

        </style>
    </head>
    <body>
         
        <!--  <canvas id="canvas" width="200" height="90"></canvas>  -->
        <h1 class="tITULO">KUONGSHUN ESP32CAM ROBOT</h1>
        <!--  <a href="https://www.youtube.com" class="LINK">YOUTUBE</a>  -->
    
        <img id="stream" src="" class="cont_stream">      

        <div class="cont_flex_stream">     
            <button type="button" id="toggle-stream">Start Screen</button> 
            <button type="button" id="get-still">Pause Screen</button> 
            <button type="button" id="close-stream">Close Screen</button>
        </div>

        <!--
        <div class="cont_flex"><div><input type="checkbox" style="margin-right: 5px;" id="nostop" onclick="var noStop=0;if (this.checked) noStop=1;fetch(document.location.origin+'/control?var=nostop&val='+noStop);">No Stop</div></div>
        -->
        
        <div class="cont_flex_threebuttom">     
            <button type="button" id="leftup" ontouchstart="fetch(document.location.origin+'/control?var=car&val=6');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Leftup</button>
            <button type="button" id="forward" ontouchstart="fetch(document.location.origin+'/control?var=car&val=1');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Forward</button>
            <button type="button" id="rightup" ontouchstart="fetch(document.location.origin+'/control?var=car&val=7');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Rightup</button>
        </div>

        <div class="cont_flex_twobuttom">     
            <button type="button" id="turnleft" ontouchstart="fetch(document.location.origin+'/control?var=car&val=4');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">TurnLeft</button>
            <!--    <button type="button" id="Stop" ontouchstart="fetch(document.location.origin+'/control?var=car&val=3');">Stop</button>    -->
            <button type="button" id="turnright" ontouchstart="fetch(document.location.origin+'/control?var=car&val=2');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">TurnRight</button>  
        </div>

        <div class="cont_flex_threebuttom">     
            <button type="button" id="leftdown" ontouchstart="fetch(document.location.origin+'/control?var=car&val=8');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Leftdown</button>
            <button type="button" id="backward" ontouchstart="fetch(document.location.origin+'/control?var=car&val=5');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Backward</button>
            <button type="button" id="rightdown" ontouchstart="fetch(document.location.origin+'/control?var=car&val=9');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Rightdown</button>
        </div>

        <div class="cont_flex_twobuttom">     
            <button type="button" id="clockwise" ontouchstart="fetch(document.location.origin+'/control?var=car&val=10');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Clockwise</button>
            <button type="button" id="Contrarotate" ontouchstart="fetch(document.location.origin+'/control?var=car&val=15');"ontouchend="fetch(document.location.origin+'/control?var=car&val=3');">Contrarotate</button>  
        </div>

        <div class="cont_flex_model">     
            <button type="button" id="model1" onmousedown="fetch(document.location.origin+'/control?var=car&val=11');">Free Control</button>
            <button type="button" id="model2" onmousedown="fetch(document.location.origin+'/control?var=model&val=2');">Obstacle Avoidance</button>
        </div>

        <div class="cont_flex_model">     
            <button type="button" id="model3" onmousedown="fetch(document.location.origin+'/control?var=model&val=3');">Object Following</button>
            <button type="button" id="model4" onmousedown="fetch(document.location.origin+'/control?var=model&val=4');">Line Tracing</button>
        </div>

        
        <div class="cont_flex">  
            <div style="display: flex;align-items: center;">Servo <input type="range" id="servo" min="0" max="180" value="90" onchange="try{fetch(document.location.origin+'/control?var=servo&val='+this.value);}catch(e){}"></div>
        </div>
        
        <div class="cont_flex">  
            <div style="display: flex;align-items: center;">Speed <input type="range" id="speed" min="150" max="255" value="220" onchange="try{fetch(document.location.origin+'/control?var=speed&val='+this.value);}catch(e){}"></div>
        </div>
        <div class="cont_flex">  
            <div style="display: flex;align-items: center;">L E D <input type="range" id="flash" min="0" max="255" value="0" onchange="try{fetch(document.location.origin+'/control?var=flash&val='+this.value);}catch(e){}"></div>
        </div>
        <!--
        <div class="cont_flex">  
            <div style="display: flex;align-items: center;">Qual. <input type="range" id="quality" min="10" max="63" value="10" onchange="try{fetch(document.location.origin+'/control?var=quality&val='+this.value);}catch(e){}"></div>
        </div>
        <div class="cont_flex">  
            <div style="display: flex;align-items: center;">Frame <input type="range" id="framesize" min="0" max="5" value="5" onchange="try{fetch(document.location.origin+'/control?var=framesize&val='+this.value);}catch(e){}"></div>
        </div>
        -->

        <script>
            window.onload = function(){
                var canvas = document.getElementById("canvas");
                var ctx = canvas.getContext("2d");

                ctx.fillStyle = "rgb(255,0,0)";
                ctx.fillRect(73,25,60,35);
                ctx.clearRect(78,30,50,25);

                ctx.fillRect(93,20,20,5);
                ctx.fillRect(68,35,5,15);
                ctx.fillRect(133,35,5,15);

                ctx.beginPath();
                ctx.arc(92,42,6,0,2*Math.PI,true);
                ctx.fill();

                ctx.beginPath();
                ctx.arc(117,42,6,0,2*Math.PI,true);
                ctx.fill();

                ctx.beginPath();
                ctx.arc(104,100,35,0,Math.PI,true);
                ctx.fill();

                ctx.clearRect(50,85,100,20);

            }
        
            document.addEventListener(
            'DOMContentLoaded',function(){
                function b(B){let C;switch(B.type){case'checkbox':C=B.checked?1:0;break;case'range':case'select-one':C=B.value;break;case'button':case'submit':C='1';break;default:return;}const D=`${c}/control?var=${B.id}&val=${C}`;fetch(D).then(E=>{console.log(`request to ${D} finished, status: ${E.status}`)})}var c=document.location.origin;const e=B=>{B.classList.add('hidden')},f=B=>{B.classList.remove('hidden')},g=B=>{B.classList.add('disabled'),B.disabled=!0},h=B=>{B.classList.remove('disabled'),B.disabled=!1},i=(B,C,D)=>{D=!(null!=D)||D;let E;'checkbox'===B.type?(E=B.checked,C=!!C,B.checked=C):(E=B.value,B.value=C),D&&E!==C?b(B):!D&&('aec'===B.id?C?e(v):f(v):'agc'===B.id?C?(f(t),e(s)):(e(t),f(s)):'awb_gain'===B.id?C?f(x):e(x):'face_recognize'===B.id&&(C?h(n):g(n)))};document.querySelectorAll('.close').forEach(B=>{B.onclick=()=>{e(B.parentNode)}}),fetch(`${c}/status`).then(function(B){return B.json()}).then(function(B){document.querySelectorAll('.default-action').forEach(C=>{i(C,B[C.id],!1)})});const j=document.getElementById('stream'),k=document.getElementById('stream-container'),l=document.getElementById('get-still'),m=document.getElementById('toggle-stream'),n=document.getElementById('face_enroll'),o=document.getElementById('close-stream'),p=()=>{window.stop(),m.innerHTML='Start Screen'},q=()=>{j.src=`${c+':81'}/stream`,f(k),m.innerHTML='Stop Stream'};l.onclick=()=>{p(),j.src=`${c}/capture?_cb=${Date.now()}`,f(k)},o.onclick=()=>{p(),e(k)},m.onclick=()=>{const B='Stop Stream'===m.innerHTML;B?p():q()},n.onclick=()=>{b(n)},document.querySelectorAll('.default-action').forEach(B=>{B.onchange=()=>b(B)});const r=document.getElementById('agc'),s=document.getElementById('agc_gain-group'),t=document.getElementById('gainceiling-group');r.onchange=()=>{b(r),r.checked?(f(t),e(s)):(e(t),f(s))};const u=document.getElementById('aec'),v=document.getElementById('aec_value-group');u.onchange=()=>{b(u),u.checked?e(v):f(v)};const w=document.getElementById('awb_gain'),x=document.getElementById('wb_mode-group');w.onchange=()=>{b(w),w.checked?f(x):e(x)};const y=document.getElementById('face_detect'),z=document.getElementById('face_recognize'),A=document.getElementById('framesize');A.onchange=()=>{b(A),5<A.value&&(i(y,!1),i(z,!1))},y.onchange=()=>{return 5<A.value?(alert('Please select CIF or lower resolution before enabling this feature!'),void i(y,!1)):void(b(y),!y.checked&&(g(n),i(z,!1)))},z.onchange=()=>{return 5<A.value?(alert('Please select CIF or lower resolution before enabling this feature!'),void i(z,!1)):void(b(z),z.checked?(h(n),i(y,!0)):g(n))}});
        
        </script>
    </body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

   httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };
    
    Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
