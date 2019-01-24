#include <sdk/config.h>
#include <stdio.h>
#include <arch/board/board.h>
#include <arch/chip/pin.h>

#include <MediaRecorder.h>
#include <MemoryUtil.h>

#include <SPI.h>

MediaRecorder *theRecorder;

// static const int32_t recoding_frames = 400;
static const int32_t buffer_size = 6144;
static uint8_t s_buffer[buffer_size];

bool ErrEnd = false;

const int led_pin[4] = {
    LED0,
    LED1,
    LED2,
    LED3
    };

#define PIN_FLOWCONTROL PIN_D08

/**
 * @brief Audio attention callback
 *
 * When audio internal error occurc, this function will be called back.
 */

void mediarecorder_attention_cb(const ErrorAttentionParam *atprm)
{
  puts("Attention!");

  if (atprm->error_code >= AS_ATTENTION_CODE_WARNING)
  {
    ErrEnd = true;
  }
}

/**
 * @brief Recorder done callback procedure
 *
 * @param [in] event        AsRecorderEvent type indicator
 * @param [in] result       Result
 * @param [in] sub_result   Sub result
 *
 * @return true on success, false otherwise
 */

static bool mediarecorder_done_callback(AsRecorderEvent event, uint32_t result, uint32_t sub_result)
{
  printf("mp cb %x %x %x\n", event, result, sub_result);

  return true;
}

bool buffer_flag = false;
uint32_t b_size = 0;
int led_now = 0;

void nightrider()
{
  digitalWrite(led_pin[led_now], LOW);
  led_now++;
  if (led_now > 3)
  {
    led_now = 0;
  }
  digitalWrite(led_pin[led_now], HIGH);
}

/**
 * @brief Audio signal process (Modify for your application)
 */

int spicnt = 0;
long acm_cnt = 0;

int spi_send_finished_flag = 1;
int spi_err_cnt_flag = 0;

void sendSPI(uint32_t size)
{
  spi_send_finished_flag = 0;
  int offset = 0;
  acm_cnt = acm_cnt + size;

  for (int i = 0; i < 8; i++)
  {
    // s_buffer[i] = i + spicnt;//for test signal
  }
  spicnt++;

  printf("Size\t%d\t[%02x %02x %02x %02x %02x %02x %02x %02x ...]\t%d\tus\t%d\t",
         size,
         s_buffer[offset + 0],
         s_buffer[offset + 1],
         s_buffer[offset + 2],
         s_buffer[offset + 3],
         s_buffer[offset + 4],
         s_buffer[offset + 5],
         s_buffer[offset + 6],
         s_buffer[offset + 7],
         micros(), acm_cnt);

  long us_b = micros();
  int sent_bytes = 0;
  int sendsize_once = 2048;

  SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE1));
  while (sent_bytes < size)
  {
    while (digitalRead(PIN_FLOWCONTROL) == 0)
    {
    }
    delayMicroseconds(20);
    noInterrupts();
    SPI.send(&s_buffer[sent_bytes], sendsize_once);
    interrupts();
    sent_bytes = sent_bytes + sendsize_once;
    delayMicroseconds(30);
  }
  SPI.endTransaction();

  spi_send_finished_flag = 1;

  printf("%d\tB\t%d\tus\n", acm_cnt, micros() - us_b);

  nightrider();
}

void toggle_led(uint32_t pin)
{
  board_gpio_write(pin, board_gpio_read(pin) == 0 ? 1 : 0);
}

void signal_process(uint32_t size)
{
  if (spi_send_finished_flag == 0)
  {
    printf("ERR CNT %d\n", spi_err_cnt_flag++);
  }

  sendSPI(size);
  if (1)
  {
    if (buffer_flag)
    {
      // printf("Already buffer full.\n");
    }
    b_size = size;
    buffer_flag = true;
  }
}

/**
 * @brief Execute frames for FIFO empty
 */

void execute_frames()
{
  uint32_t read_size = 0;
  do
  {
    err_t err = execute_aframe(&read_size);
    if ((err != MEDIARECORDER_ECODE_OK) && (err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA))
    {
      break;
    }
  } while (read_size > 0);
}

/**
 * @brief Execute one frame
 */

err_t execute_aframe(uint32_t *size)
{
  err_t err = theRecorder->readFrames(s_buffer, buffer_size, size);
  if (((err == MEDIARECORDER_ECODE_OK) || (err == MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)) && (*size > 0))
  {
    signal_process(*size);
  }
  return err;
}

/**
 *  @brief Setup audio device to capture PCM stream
 *
 *  Select input device as microphone <br>
 *  Set PCM capture sapling rate parameters to 48 kb/s <br>
 *  Set channel number 4 to capture audio from 4 microphones simultaneously <br>
 *  System directory "/mnt/sd0/BIN" will be searched for PCM codec
 */
void setup()
{
  /* Initialize memory pools and message libs */
  initMemoryPools();
  createStaticPools(MEM_LAYOUT_RECORDER);

  // setup
  Serial.begin(115200);
  pinMode(PIN_FLOWCONTROL, INPUT_PULLDOWN);

  pthread_t thread_A, thread_B;
  for (int i = 0; i < 4; i++)
  {
    pinMode(led_pin[i], OUTPUT);
  }
  SPI.begin();

  /* start audio system */
  theRecorder = MediaRecorder::getInstance();
  theRecorder->begin(mediarecorder_attention_cb);
  puts("initialization MediaRecorder");

  /* Set capture clock */
  theRecorder->setCapturingClkMode(MEDIARECORDER_CAPCLK_NORMAL);

  /* Activate Objects. Set output device to Speakers/Headphones */
  theRecorder->activate(AS_SETRECDR_STS_INPUTDEVICE_MIC, mediarecorder_done_callback);
  usleep(100 * 1000); /* waiting for Mic startup */

  /*
   * Initialize recorder to decode stereo wav stream with 48kHz sample rate
   * Search for SRC filter in "/mnt/sd0/BIN" directory
   */
  theRecorder->init(AS_CODECTYPE_LPCM,
                    AS_CHANNEL_4CH,
                    AS_SAMPLINGRATE_16000,
                    AS_BITLENGTH_16,
                    AS_BITRATE_8000,  /* Bitrate is effective only when mp3 recording */
                    "/mnt/spif/BIN"); //sd0
  theRecorder->start();
  puts("Recording Start!");
}

/**
 * @brief Capture frames of PCM data into buffer
 */

void loop()
{

  static int32_t total_size = 0;
  uint32_t read_size = 0;

  /* Execute audio data */
  err_t err = execute_aframe(&read_size);
  if (err != MEDIARECORDER_ECODE_OK && err != MEDIARECORDER_ECODE_INSUFFICIENT_BUFFER_AREA)
  {
    puts("Recording Error!");
    theRecorder->stop();
    goto exitRecording;
  }
  else if (read_size > 0)
  {
    total_size += read_size;
  }

  /* This sleep is adjusted by the time to write the audio stream file.
     Please adjust in according with the processing contents
     being processed at the same time by Application.
  */
  // usleep(10000);

  /* Stop Recording */
  // if (total_size > (recoding_frames * buffer_size) && FALSE)
  {
    theRecorder->stop();

    /* Get ramaining data(flushing) */
    sleep(1); /* For data pipline stop */
    execute_frames();

    goto exitRecording;
  }

  if (ErrEnd)
  {
    printf("Error End\n");
    theRecorder->stop();
    goto exitRecording;
  }

  return;

exitRecording:

  theRecorder->deactivate();
  theRecorder->end();

  puts("End Recording");
  exit(1);
}
