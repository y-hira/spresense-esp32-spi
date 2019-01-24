/*
 * This sketch sends data which come from HSPI to WiFi with UDP.
 * Board: ESP32 Dev Module
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "lwip/igmp.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "soc/rtc_cntl_reg.h"
#include "rom/cache.h"
#include "driver/spi_slave.h"
#include "esp_log.h"
#include "esp_spi_flash.h"

#include <WiFi.h>
#include <WiFiUdp.h>
const char *ssid = "your-ssid";
const char *pwd = "your-ssid";
const char *udpAddress = "192.168.8.100";
const int udpPort = 44444;
WiFiUDP udp;

/*
Pins in use. The SPI Master can use the GPIO mux, so feel free to change these if needed.
*/
#define GPIO_MOSI 13
#define GPIO_MISO 12
#define GPIO_SCLK 14
#define GPIO_CS 15
#define GPIO_LED 32
#define GPIO_FLOWCONTROL 5

esp_err_t ret;
spi_slave_transaction_t t;

static const int32_t buffer_size = 3072;
uint8_t sendbuf[buffer_size] = "";
uint8_t recvbuf[buffer_size] = "";
char packetBuffer[50]="";
int n = 0;

//Called after a transaction is queued and ready for pickup by master. We use this to set the handshake line high.
void my_post_setup_cb(spi_slave_transaction_t *trans)
{
}

//Called after transaction is sent/received. We use this to set the handshake line low.
void my_post_trans_cb(spi_slave_transaction_t *trans)
{
}

void setup()
{
    Serial.begin(115200);
    //Configuration for the SPI bus
    spi_bus_config_t buscfg = {
        mosi_io_num : GPIO_MOSI,
        miso_io_num : GPIO_MISO,
        sclk_io_num : GPIO_SCLK
    };

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        spics_io_num : GPIO_CS,
        flags : 0,
        queue_size : 3,
        mode : 0,
        post_setup_cb : my_post_setup_cb,
        post_trans_cb : my_post_trans_cb
    };

    pinMode(GPIO_MOSI, INPUT_PULLUP);
    pinMode(GPIO_SCLK, INPUT_PULLUP);
    pinMode(GPIO_CS, INPUT_PULLUP);
    pinMode(GPIO_FLOWCONTROL, OUTPUT);
    digitalWrite(GPIO_FLOWCONTROL, LOW);

    //Initialize SPI slave interface
    ret = spi_slave_initialize(HSPI_HOST, &buscfg, &slvcfg, 1);
    assert(ret == ESP_OK);

    memset(recvbuf, 0, 33);
    memset(&t, 0, sizeof(t));

    // WiFi Settings
    WiFi.begin(ssid, pwd);
    Serial.println("");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    udp.begin(udpPort);

    pinMode(GPIO_LED, OUTPUT);
    digitalWrite(GPIO_LED, LOW);
}

long acm_cnt = 0;
int trans_len;

void loop()
{
    //Clear receive buffer, set send buffer to something sane
    memset(recvbuf, 0xA5, buffer_size);
    t.length = buffer_size * 8;
    t.tx_buffer = sendbuf;
    t.rx_buffer = recvbuf;
    /* This call enables the SPI slave interface to send/receive to the sendbuf and recvbuf. The transaction is
        initialized by the SPI master, however, so it will not actually happen until the master starts a hardware transaction
        by pulling CS low and pulsing the clock etc. In this specific example, we use the handshake line, pulled up by the
        .post_setup_cb callback that is called as soon as a transaction is ready, to let the master know it is free to transfer
        data.
        */
    digitalWrite(GPIO_FLOWCONTROL, HIGH);
    ret = spi_slave_transmit(HSPI_HOST, &t, portMAX_DELAY);
    digitalWrite(GPIO_FLOWCONTROL, LOW);

    trans_len = t.trans_len;
    t.trans_len = 0;

    if (trans_len > (buffer_size - 1) * 8 || trans_len > 2047)
    {
        writeudp(trans_len/8);
    }
    else if(trans_len==1 && recvbuf[0]==0xA4){
        Serial.println("----");
    }else{
        Serial.print((byte)recvbuf[0], HEX);
        Serial.println("\t" + String(trans_len, DEC));
    }

    acm_cnt = acm_cnt + trans_len;
    n++;
}


void writeudp(long _bf_size)
{
    packetBuffer[0]=0;
    digitalWrite(GPIO_LED, HIGH);
    udp.beginPacket(udpAddress, udpPort);
    udp.write(recvbuf, _bf_size);
    udp.endPacket();
    udp.parsePacket();
  if(udp.read(packetBuffer, 50) > 0){
    Serial.print("RECV ");
    Serial.println((char *)packetBuffer);
  }
    Serial.print("SENT "+String(_bf_size,DEC)+"\t"+String(packetBuffer[0],DEC)+"\t");
    Serial.print((byte)recvbuf[0], HEX);
    Serial.println("\t" + String(trans_len, DEC) + "");
    t.trans_len = 0;
    digitalWrite(GPIO_LED, LOW);
}
