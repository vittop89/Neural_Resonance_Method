/* =============================================================================
   NEURAL RESONANCE METHOD  -  firmware v3.0  -  ESP32
   Biofeedback respiratorio a circuito chiuso su tre canali sensoriali:
   TATTO (40 Hz reali) - LUCE (colore e saturazione) - SUONO (sintesi software).

   Target : ESP32-WROOM-32, scheda DevKit a 38 pin
   Core   : arduino-esp32 2.0.x  (vedi NOTA CORE in fondo per la 3.x)
   Licenza: uso personale / sperimentale.  NON e' un dispositivo medico.

   -----------------------------------------------------------------------------
   COSA CAMBIA RISPETTO ALLA v2 SU ARDUINO UNO
   -----------------------------------------------------------------------------
   Sull'Uno meta' del progetto erano aggiramenti di limiti della scheda. Qui
   spariscono tutti, e con loro tre componenti:

   - IL SUONO E' SINTETIZZATO QUI DENTRO. Niente piu' VS1053 ne' MIDI: un DAC
     I2S riceve un flusso stereo a 22050 Hz generato da un task dedicato sul
     core 0. Drone, letto di risacca e campane sono oscillatori veri, con
     filtro e inviluppi, non timbri General MIDI.

   - I 40 Hz ESCONO DA UN DAC, NON DAL PWM. L'ESP32 ha due convertitori
     digitale-analogico veri su GPIO25 e GPIO26. Sparisce la portante a
     31 kHz, e con lei tutta la rete RC + attenuatore che serviva a
     cancellarla: resta un partitore per il livello e un condensatore di
     blocco. Sparisce anche la prova in continua a tre punti.

   - NIENTE ESPANSORE PWM. Il LEDC dell'ESP32 ha 16 canali indipendenti: i
     tre dei LED e i due dei motorini stanno comodi, senza PCA9685 e senza
     spartirsi i timer.

   - IL BATTITO E' DIGITALE. Il MAX30102 fa la sua acquisizione a bordo e
     parla I2C a 3,3 V, cioe' proprio la logica dell'ESP32. Sparisce la
     catena analogica del Pulse Sensor con il suo filtro di alimentazione.

   - TELEMETRIA SEMPRE DISPONIBILE. Tre UART invece di una: la seriale non
     serve piu' al MIDI e resta libera.

   -----------------------------------------------------------------------------
   MAPPA PIN  -  vincolata dai limiti reali dell'ESP32
   -----------------------------------------------------------------------------
   Tre regole hanno deciso quasi tutto:
     1. ADC2 NON funziona quando il Wi-Fi e' attivo -> ogni ingresso analogico
        deve stare su ADC1 (GPIO 32-39).
     2. I DAC esistono solo su GPIO25 e GPIO26. Non sono spostabili.
     3. GPIO 6-11 sono la flash SPI; 34-39 sono solo ingresso e senza pull-up;
        0, 2, 12, 15 sono pin di strapping e vanno trattati con riguardo.

     GPIO 34  IN   FSR402, partitore ......... respiro   [ADC1_CH6, solo input]
     GPIO 21  I/O  SDA  ⎫ MAX30102, battito. Il sensore e' a 3,3 V come
     GPIO 22  OUT  SCL  ⎭ l'ESP32: nessun traslatore.
     GPIO 25  OUT  DAC1 -> partitore -> ampli L ... trasduttore STERNO
     GPIO 26  OUT  DAC2 -> partitore -> ampli R ... trasduttore ADDOME
     GPIO 18  OUT  I2S BCK   ⎫
     GPIO 19  OUT  I2S LRCK  ⎬ PCM5102A, uscita audio di linea
     GPIO 23  OUT  I2S DIN   ⎭
     GPIO 16  OUT  LED rosso   ⎫ tutti e tre passano dal 74HCT245, che porta
     GPIO 17  OUT  LED verde   ⎬ i 3,3 V a 5 V pieni sui gate dei MOSFET
     GPIO 4   OUT  LED blu     ⎭
     GPIO 27  OUT  motorino sterno  ⎫ solo disposizione ibrida, anch'essi
     GPIO 13  OUT  motorino addome  ⎭ attraverso il 74HCT245
     GPIO 33  IN   ponticello sham verso GND (pull-up interno)
     GPIO 2   OUT  LED di stato integrato sulla scheda

     liberi: 5, 14, 15, 32, 35, 36, 39
   ============================================================================= */

#include <Wire.h>
#include <Preferences.h>          /* registro sessioni in flash               */
#include <driver/i2s.h>           /* uscita audio                             */
#include <esp_task_wdt.h>         /* watchdog                                 */
#include "MAX30105.h"             /* libreria SparkFun MAX3010x               */


/* ===========================================================================
   1.  CONFIGURAZIONE
   =========================================================================== */

#define TELEMETRIA          1   /* CSV a 115200: sempre disponibile su ESP32  */
#define PHASE_LOCK          1   /* azzera la fase di risonanza a ogni battito */
#define DISPOSIZIONE_IBRIDA 0   /* 1 = due motorini sulla fascia toracica     */
#define SHAM_ABILITATO      1   /* ponticello GPIO33-GND = sessione cieca     */
#define INGRESSI_SINTETICI  0   /* 1 = respiro e battito generati dal firmware*/


/* ===========================================================================
   2.  PIN
   =========================================================================== */

const uint8_t PIN_FSR       = 34;
const uint8_t PIN_I2C_SDA   = 21;
const uint8_t PIN_I2C_SCL   = 22;
const uint8_t PIN_TAT_ALTO  = 25;   /* DAC1 */
const uint8_t PIN_TAT_BASSO = 26;   /* DAC2 */
const uint8_t PIN_I2S_BCK   = 18;
const uint8_t PIN_I2S_LRCK  = 19;
const uint8_t PIN_I2S_DIN   = 23;
const uint8_t PIN_LED_R     = 16;
const uint8_t PIN_LED_G     = 17;
const uint8_t PIN_LED_B     = 4;
const uint8_t PIN_MOT_ALTO  = 27;
const uint8_t PIN_MOT_BASSO = 13;
const uint8_t PIN_SHAM      = 33;
const uint8_t PIN_STATUS    = 2;

/* canali LEDC: indipendenti, nessun timer da spartire */
const uint8_t CH_R = 0, CH_G = 1, CH_B = 2, CH_MA = 3, CH_MB = 4;
const uint32_t LEDC_FREQ = 4000;
const uint8_t  LEDC_BIT  = 12;      /* 0..4095: 16 volte la risoluzione AVR   */


/* ===========================================================================
   3.  COSTANTI DI TEMPO E SOGLIE
   =========================================================================== */

const uint32_t T_CALIB_MS       = 10000;
const uint32_t T_SETTLE_MS      = 500;
const uint32_t T_RESP_MS        = 20;      /* respiro a 50 Hz                 */
const uint32_t T_TELEM_MS       = 100;
const uint32_t T_REFRATTARIO_MS = 500;     /* tetto 120 BPM                   */
const uint32_t T_IBI_MAX_MS     = 2000;
const uint32_t T_PULSE_TOUT_MS  = 3000;    /* SICUREZZA                       */
const uint32_t T_FSR_FAULT_MS   = 2000;
const uint32_t T_BLINK_MS       = 250;
const uint32_t T_FLASH_MS       = 60;
const uint32_t SESS_MIN_MS      = 120000;

const float F_GAMMA = 40.0f, F_MIN = 30.0f, F_MAX = 50.0f;

/* --- tatto: uscita DAC a 8 bit centrata a meta' scala ---------------------- */
const uint8_t TAT_RIPOSO   = 128;
const uint8_t TAT_MAX      = 100;   /* semiampiezza max: 100/128 della corsa  */
const float   TAT_DEADZONE = 0.03f;
const uint32_t TAT_SR      = 4000;  /* aggiornamenti DAC al secondo          */

/* --- colori ---------------------------------------------------------------- */
const uint8_t COL_INSP [3] = {   0,  60, 255 };
const uint8_t COL_ESP  [3] = { 255,  70,   0 };
const uint8_t COL_CAL  [3] = {  80,   0, 160 };
const uint8_t COL_ALERT[3] = { 255, 210,   0 };
const uint8_t COL_FAULT[3] = { 255,   0, 120 };
const float LUM_MIN = 0.20f, SAT_MIN = 0.25f;

/* --- respiro ---------------------------------------------------------------- */
const float   A_VELOCE = 0.25f, A_LENTA = 0.06f, SLEW_FLOW = 0.06f;
const int16_t RANGE_MIN = 200;      /* ADC a 12 bit: fondoscala 4095          */

/* --- battito ---------------------------------------------------------------- */
const int32_t PPG_AMP_MIN     = 1500;   /* il MAX30102 da conteggi molto piu' */
const uint8_t PPG_SOGLIA_ALTA = 60;     /* grandi del Pulse Sensor            */
const uint8_t PPG_SOGLIA_BASSA= 40;
const uint8_t PPG_DECADI_OGNI = 10;
const uint8_t IBI_BUF_N       = 5;
const float   A_COERENZA      = 0.08f;

/* --- motorini (ibrida) ------------------------------------------------------ */
const uint16_t MOT_DUTY_MAX = 2400;  /* su 4095: ~3,0 V medi da un rail 5 V   */
const uint16_t MOT_DUTY_MIN = 1100;
const float    MOT_DEADZONE = 0.05f;
const float    MOT_FONDO    = 0.35f;


/* ===========================================================================
   4.  STATO
   =========================================================================== */

enum { ST_CALIB, ST_RUN, ST_NO_PULSE, ST_FSR_FAULT };
uint8_t stato = ST_CALIB;

/* --- respiro --- */
float    emaVeloce = 0, emaLenta = 0, calMin = 4095, calMax = 0;
float    profondita = 0, flusso = 0, radAlto = 0, radBasso = 1;
bool     emaInit = false, inspirazione = false, inspPrec = false;
float    profAlTurno = 0.5f;
uint32_t tFsrOk = 0, tRespPrec = 0, tShrink = 0;

/* --- battito --- */
int32_t  ppgPicco = 0, ppgValle = 0;
bool     battArmato = false;
uint8_t  decadiCnt = 0;
uint32_t tUltimoBattito = 0, tFlashLed = 0;
uint16_t ibiBuf[IBI_BUF_N]; uint8_t ibiIdx = 0, ibiCount = 0;
float    bpm = 0, ibiMedio = 0, coerenza = 0;

/* --- risonanza: letta dal task audio e dal timer del DAC ------------------- */
volatile float fRisonanza = F_GAMMA;
volatile uint16_t moltiplic = 0;

/* --- sessione --- */
bool     shamAttivo = false;
uint32_t tInizioRun = 0, tAccum = 0, tTelemPrec = 0;
float    coerenzaSomma = 0, bpmSomma = 0;
uint16_t campioniSess = 0;

/* --- condivise fra core: il controllo scrive, l'audio legge ---------------- */
volatile float audioProfondita = 0.0f;
volatile float audioFlusso     = 0.0f;
volatile float audioCoerenza   = 0.0f;
volatile bool  audioAttivo     = false;
volatile int8_t audioCampana   = 0;   /* +1 acuta, -1 grave, 0 niente        */
volatile float audioVelCampana = 0.0f;

Preferences prefs;
MAX30105 ppgSensore;
hw_timer_t *timerTat = NULL;
portMUX_TYPE muxTat = portMUX_INITIALIZER_UNLOCKED;

/* tabella seno condivisa: 512 punti, interi con segno */
int16_t tabSeno[512];


/* ===========================================================================
   5.  UTILITA'
   =========================================================================== */

static inline uint16_t gammaCorr(uint8_t v) {
  /* gamma 2 su 12 bit: (v/255)^2 * 4095                                      */
  return (uint16_t)(((uint32_t)v * (uint32_t)v * 4095UL) / (255UL * 255UL));
}

void costruisciTabellaSeno() {
  for (uint16_t i = 0; i < 512; i++)
    tabSeno[i] = (int16_t)(sinf((float)i * 6.28318531f / 512.0f) * 32000.0f);
}

/* rumore bianco veloce: xorshift a 32 bit, niente moltiplicazioni            */
uint32_t rngStato = 0x12345678;
static inline int16_t rumore() {
  rngStato ^= rngStato << 13; rngStato ^= rngStato >> 17; rngStato ^= rngStato << 5;
  return (int16_t)(rngStato >> 16);
}

void scriviLed(uint8_t r, uint8_t g, uint8_t b) {
  static uint8_t rP = 1, gP = 1, bP = 1;
  if (r == rP && g == gP && b == bP) return;
  rP = r; gP = g; bP = b;
  ledcWrite(CH_R, gammaCorr(r));
  ledcWrite(CH_G, gammaCorr(g));
  ledcWrite(CH_B, gammaCorr(b));
}


/* ===========================================================================
   6.  CANALE TATTILE  -  interrupt di timer sul DAC
   ---------------------------------------------------------------------------
   Qui sta la semplificazione piu' grande rispetto alla versione AVR. Il DAC
   produce una tensione analogica vera, quindi non c'e' nessuna portante da
   filtrare: la rete esterna si riduce a un partitore per il livello e a un
   condensatore che blocca la continua.

   L'interrupt gira a 4 kHz: cento campioni per ogni ciclo da 40 Hz, contro i
   pochi che il PWM a 8 bit riusciva a dare.
   =========================================================================== */

volatile uint32_t faseAcc = 0, faseInc = 0;
volatile uint8_t  ampAlto = 0, ampBasso = 0;   /* 0..TAT_MAX                  */

void IRAM_ATTR isrTattile() {
  portENTER_CRITICAL_ISR(&muxTat);
  faseAcc += faseInc;
  uint16_t idx = (uint16_t)(faseAcc >> 23) & 0x1FF;   /* 512 punti           */
  int16_t  s   = tabSeno[idx];
  uint8_t  aA  = ampAlto, aB = ampBasso;
  portEXIT_CRITICAL_ISR(&muxTat);

  /* seno con segno -> escursione attorno a meta' scala del DAC              */
  int16_t vA = TAT_RIPOSO + (int16_t)(((int32_t)s * aA) >> 15);
  int16_t vB = TAT_RIPOSO + (int16_t)(((int32_t)s * aB) >> 15);
  dacWrite(PIN_TAT_ALTO,  (uint8_t)constrain(vA, 0, 255));
  dacWrite(PIN_TAT_BASSO, (uint8_t)constrain(vB, 0, 255));
}

/* incremento di fase: un giro = 2^32, campionamento a TAT_SR                 */
void aggiornaFaseInc() {
  portENTER_CRITICAL(&muxTat);
  faseInc = (uint32_t)((fRisonanza / (float)TAT_SR) * 4294967296.0f);
  portEXIT_CRITICAL(&muxTat);
}

uint8_t livelloTattile(float k) {
  if (shamAttivo)       return 0;      /* sessione cieca: nessun 40 Hz       */
  if (k < TAT_DEADZONE) return 0;
  if (k > 1.0f) k = 1.0f;
  return (uint8_t)((float)TAT_MAX * k);
}


/* ===========================================================================
   7.  CANALE SUONO  -  sintesi software su I2S
   ---------------------------------------------------------------------------
   Tre voci generate qui, non timbri di una tabella General MIDI:

   DRONE   due sinusoidi a 41,2 e 82,4 Hz. La prima e' praticamente il target
           dei 40 Hz: il corpo sente il fondamentale sulla pelle e le orecchie
           lo stesso fondamentale piu' l'ottava. Volume dalla profondita' del
           respiro, e un passa-basso a un polo la cui frequenza segue la
           direzione: apre inspirando, chiude espirando. E' il filtro, non il
           volume, a far sembrare il suono un respiro.

   LETTO   rumore bianco filtrato due volte: risacca. Volume inverso alla
           profondita', cosi' la sonorita' complessiva resta costante e a
           cambiare e' il timbro.

   CAMPANE due parziali con inviluppo esponenziale, al cambio di direzione.
   =========================================================================== */

const uint32_t AUDIO_SR   = 22050;
const size_t   AUDIO_BLOC = 256;      /* frame per scrittura I2S              */

void taskAudio(void *param) {
  static int16_t buf[AUDIO_BLOC * 2];

  uint32_t fase1 = 0, fase2 = 0, faseB1 = 0, faseB2 = 0;
  float lpDrone = 0, lpBed1 = 0, lpBed2 = 0;
  float envCampana = 0.0f;

  const uint32_t inc1 = (uint32_t)((41.2f  / AUDIO_SR) * 4294967296.0f);
  const uint32_t inc2 = (uint32_t)((82.4f  / AUDIO_SR) * 4294967296.0f);

  uint32_t incB1 = 0, incB2 = 0;

  for (;;) {
    float prof = audioProfondita, flu = audioFlusso;
    bool  on   = audioAttivo;

    /* evento campana, consumato una volta sola */
    int8_t ev = audioCampana;
    if (ev != 0) {
      audioCampana = 0;
      envCampana = audioVelCampana;
      float f = (ev > 0) ? 659.3f : 329.6f;          /* E5 al vertice, E4 al fondo */
      incB1 = (uint32_t)((f         / AUDIO_SR) * 4294967296.0f);
      incB2 = (uint32_t)((f * 2.76f / AUDIO_SR) * 4294967296.0f); /* parziale inarmonica */
      faseB1 = faseB2 = 0;
    }

    /* volumi: il letto avanza mentre il drone si ritira                      */
    float volDrone = on ? (0.25f + 0.75f * prof) * 0.42f : 0.0f;
    float volBed   = on ? (1.0f - 0.6f * prof)   * 0.22f : 0.0f;

    /* brillantezza: coefficiente del passa-basso, dalla direzione            */
    float kDrone = 0.02f + 0.14f * flu;

    for (size_t i = 0; i < AUDIO_BLOC; i++) {
      fase1 += inc1; fase2 += inc2;
      float d = (float)tabSeno[(fase1 >> 23) & 0x1FF] * 0.7f
              + (float)tabSeno[(fase2 >> 23) & 0x1FF] * 0.3f;
      lpDrone += (d - lpDrone) * kDrone;

      float n = (float)rumore();
      lpBed1 += (n      - lpBed1) * 0.05f;            /* due poli in cascata  */
      lpBed2 += (lpBed1 - lpBed2) * 0.05f;

      float c = 0.0f;
      if (envCampana > 0.0005f) {
        faseB1 += incB1; faseB2 += incB2;
        c = ((float)tabSeno[(faseB1 >> 23) & 0x1FF] * 0.75f
           + (float)tabSeno[(faseB2 >> 23) & 0x1FF] * 0.25f) * envCampana;
        envCampana *= 0.99988f;                       /* decadimento ~1,6 s   */
      }

      float m = lpDrone * volDrone + lpBed2 * volBed * 6.0f + c * 0.30f;
      if (m >  32000.0f) m =  32000.0f;
      if (m < -32000.0f) m = -32000.0f;
      int16_t s = (int16_t)m;
      buf[i * 2] = s; buf[i * 2 + 1] = s;             /* mono su due canali   */
    }

    size_t scritti;
    i2s_write(I2S_NUM_0, buf, sizeof(buf), &scritti, portMAX_DELAY);
  }
}

void avviaAudio() {
  i2s_config_t cfg = {};
  cfg.mode                = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate         = AUDIO_SR;
  cfg.bits_per_sample     = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format      = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format= I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags    = 0;
  cfg.dma_buf_count       = 8;
  cfg.dma_buf_len         = AUDIO_BLOC;
  cfg.use_apll            = false;
  cfg.tx_desc_auto_clear  = true;

  i2s_pin_config_t pins = {};
  pins.bck_io_num   = PIN_I2S_BCK;
  pins.ws_io_num    = PIN_I2S_LRCK;
  pins.data_out_num = PIN_I2S_DIN;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);

  /* il task audio vive sul core 0, il controllo resta sul core 1            */
  xTaskCreatePinnedToCore(taskAudio, "audio", 4096, NULL, 5, NULL, 0);
}


/* ===========================================================================
   8.  MOLTIPLICATORE ARMONICO
   =========================================================================== */

void aggiornaRisonanza() {
  float hz = bpm / 60.0f;
  if (hz < 0.4f) hz = 0.4f;
  uint16_t m = (uint16_t)((F_GAMMA / hz) + 0.5f);
  if (m < 1) m = 1;
  float f = hz * (float)m;
  if (f < F_MIN)      { m += 1;                f = hz * (float)m; }
  else if (f > F_MAX) { if (m > 1) { m -= 1; } f = hz * (float)m; }
  if (f < F_MIN) f = F_MIN;
  if (f > F_MAX) f = F_MAX;
  moltiplic = m; fRisonanza = f;
  aggiornaFaseInc();
}


/* ===========================================================================
   9.  REGISTRO SESSIONI  -  in flash, non piu' in EEPROM
   =========================================================================== */

void salvaSessione(uint32_t durataMs) {
  if (campioniSess == 0) return;
  uint32_t n = prefs.getUInt("n", 0);
  char chiave[16];
  snprintf(chiave, sizeof(chiave), "s%lu", (unsigned long)(n % 500));
  uint32_t rec = ((durataMs / 60000UL) & 0xFF)
               | (((uint32_t)(coerenzaSomma / campioniSess * 100.0f) & 0xFF) << 8)
               | (((uint32_t)(bpmSomma / campioniSess) & 0xFF) << 16)
               | (((uint32_t)(shamAttivo ? 1 : 0)) << 24);
  prefs.putUInt(chiave, rec);
  prefs.putUInt("n", n + 1);
}

#if TELEMETRIA
void stampaSessioni() {
  uint32_t n = prefs.getUInt("n", 0);
  Serial.println(F("# --- registro sessioni: n,minuti,coerenza%,bpm,sham ---"));
  for (uint32_t i = (n > 500 ? n - 500 : 0); i < n; i++) {
    char chiave[16];
    snprintf(chiave, sizeof(chiave), "s%lu", (unsigned long)(i % 500));
    uint32_t r = prefs.getUInt(chiave, 0);
    Serial.printf("# %lu,%lu,%lu,%lu,%lu\n", (unsigned long)i,
                  (unsigned long)(r & 0xFF), (unsigned long)((r >> 8) & 0xFF),
                  (unsigned long)((r >> 16) & 0xFF), (unsigned long)((r >> 24) & 0xFF));
  }
  Serial.println(F("# --- fine registro ---"));
}
#endif


/* ===========================================================================
   10.  INGRESSI
   =========================================================================== */

#if INGRESSI_SINTETICI
const uint32_t RESP_PERIODO_MS = 5000;
uint32_t tProxBattito = 0, tBattitoSint = 0; uint16_t ibiSint = 920;

float faseRespiroSint(uint32_t ms) { return (float)(ms % RESP_PERIODO_MS) / RESP_PERIODO_MS; }

int32_t leggiFsr(uint32_t ms) {
  return 2048 + (int32_t)(560.0f * sinf(faseRespiroSint(ms) * 6.28318531f))
       + (int32_t)(esp_random() % 17) - 8;
}

/* battito con aritmia sinusale simulata: e' il banco di prova dell'indice
   di coerenza, che con questo ingresso deve salire verso 1                   */
int32_t leggiPpg(uint32_t ms) {
  if ((int32_t)(ms - tProxBattito) >= 0) {
    tBattitoSint = tProxBattito;
    ibiSint = (uint16_t)(920.0f - 90.0f * cosf(faseRespiroSint(ms) * 6.28318531f));
    tProxBattito = tBattitoSint + ibiSint;
  }
  float ph = (float)(ms - tBattitoSint) / (float)ibiSint;
  if (ph > 1.0f) ph = 1.0f;
  float a;
  if      (ph < 0.12f) a = ph / 0.12f;
  else if (ph < 0.40f) a = 1.0f - 0.75f * (ph - 0.12f) / 0.28f;
  else if (ph < 0.52f) a = 0.25f + 0.15f * (1.0f - fabsf(ph - 0.46f) / 0.06f);
  else                 a = 0.25f * (1.0f - (ph - 0.52f) / 0.48f);
  if (a < 0) a = 0;
  return 90000 + (int32_t)(9000.0f * a);
}
#else
int32_t leggiFsr(uint32_t ms) { (void)ms; return analogRead(PIN_FSR); }
int32_t leggiPpg(uint32_t ms) { (void)ms; return (int32_t)ppgSensore.getIR(); }
#endif


/* ===========================================================================
   11.  BATTITO E COERENZA CARDIORESPIRATORIA
   =========================================================================== */

void registraIBI(uint16_t ibi) {
  if (ibiCount >= 3) {
    bool accelera = ((float)ibi < ibiMedio);
    bool atteso   = inspirazione ? accelera : !accelera;
    coerenza += ((atteso ? 1.0f : 0.0f) - coerenza) * A_COERENZA;
  }
  ibiBuf[ibiIdx] = ibi;
  ibiIdx = (ibiIdx + 1) % IBI_BUF_N;
  if (ibiCount < IBI_BUF_N) ibiCount++;
  uint32_t somma = 0;
  for (uint8_t i = 0; i < ibiCount; i++) somma += ibiBuf[i];
  ibiMedio = (float)somma / ibiCount;
  bpm = 60000.0f / ibiMedio;
  aggiornaRisonanza();
}

void taskPPG(uint32_t nowMs) {
#if !INGRESSI_SINTETICI
  if (!ppgSensore.check()) return;          /* nessun campione nuovo          */
#endif
  int32_t raw = leggiPpg(nowMs);
  if (raw < 5000) return;                   /* dito o lobo assente            */

  if (raw > ppgPicco) ppgPicco = raw;
  if (raw < ppgValle) ppgValle = raw;
  if (++decadiCnt >= PPG_DECADI_OGNI) {
    decadiCnt = 0;
    int32_t d = (ppgPicco - ppgValle) / 200;
    if (d < 1) d = 1;
    if (ppgPicco > ppgValle + 2 * d) { ppgPicco -= d; ppgValle += d; }
  }

  int32_t amp = ppgPicco - ppgValle;
  int32_t sA  = ppgValle + amp * PPG_SOGLIA_ALTA  / 100;
  int32_t sB  = ppgValle + amp * PPG_SOGLIA_BASSA / 100;

  if (!battArmato && amp >= PPG_AMP_MIN && raw > sA) {
    uint32_t dt = nowMs - tUltimoBattito;
    if (dt >= T_REFRATTARIO_MS) {
      if (tUltimoBattito != 0 && dt <= T_IBI_MAX_MS) registraIBI((uint16_t)dt);
      tUltimoBattito = nowMs; tFlashLed = nowMs; battArmato = true;
#if PHASE_LOCK
      portENTER_CRITICAL(&muxTat); faseAcc = 0; portEXIT_CRITICAL(&muxTat);
#endif
    }
  }
  if (battArmato && raw < sB) battArmato = false;
}


/* ===========================================================================
   12.  RESPIRO
   =========================================================================== */

void taskRespiro(uint32_t nowMs) {
  if ((uint32_t)(nowMs - tRespPrec) < T_RESP_MS) return;
  tRespPrec = nowMs;

  int32_t raw = leggiFsr(nowMs);
  if (raw > 20 && raw < 4075) tFsrOk = nowMs;

  if (!emaInit) { emaVeloce = raw; emaLenta = raw; emaInit = true; }
  emaVeloce += ((float)raw - emaVeloce) * A_VELOCE;
  emaLenta  += (emaVeloce  - emaLenta ) * A_LENTA;

  if (stato == ST_CALIB) {
    if (nowMs >= T_SETTLE_MS) {
      if (raw < calMin) calMin = raw;
      if (raw > calMax) calMax = raw;
    }
    return;
  }

  if (emaVeloce > calMax) calMax = emaVeloce;
  if (emaVeloce < calMin) calMin = emaVeloce;
  if ((uint32_t)(nowMs - tShrink) >= 1000) {
    tShrink = nowMs;
    if ((calMax - calMin) > (float)(RANGE_MIN + 8)) { calMax -= 4; calMin += 4; }
  }

  float range = calMax - calMin;
  if (range < (float)RANGE_MIN) range = RANGE_MIN;
  profondita = (emaVeloce - calMin) / range;
  profondita = constrain(profondita, 0.0f, 1.0f);

  float pend = emaVeloce - emaLenta;
  float banda = range / 60.0f; if (banda < 4.0f) banda = 4.0f;
  if      (pend >  banda) inspirazione = true;
  else if (pend < -banda) inspirazione = false;

  flusso += ((inspirazione ? 1.0f : 0.0f) - flusso) * SLEW_FLOW;
  radAlto  = sqrtf(flusso);
  radBasso = sqrtf(1.0f - flusso);

  /* passaggio al task audio */
  audioProfondita = profondita;
  audioFlusso     = flusso;
  audioCoerenza   = coerenza;
}


/* ===========================================================================
   13.  MACCHINA A STATI
   =========================================================================== */

void taskStato(uint32_t nowMs) {
  if (stato == ST_CALIB) {
    if (nowMs >= T_CALIB_MS) {
      if ((calMax - calMin) < (float)RANGE_MIN) {
        float c = (calMax + calMin) * 0.5f;
        calMin = c - 400; calMax = c + 400;
#if TELEMETRIA
        Serial.println(F("# escursione FSR insufficiente: range di ripiego"));
#endif
      }
      tUltimoBattito = nowMs; tFsrOk = nowMs; tShrink = nowMs;
      tInizioRun = nowMs; tAccum = nowMs;
      inspPrec = inspirazione; profAlTurno = profondita;
      stato = ST_RUN; audioAttivo = true;
    }
    return;
  }

  if ((uint32_t)(nowMs - tFsrOk) > T_FSR_FAULT_MS) { stato = ST_FSR_FAULT; audioAttivo = false; return; }
  if (stato == ST_FSR_FAULT) { stato = ST_RUN; audioAttivo = true; }

  if (stato == ST_RUN && (uint32_t)(nowMs - tAccum) >= 1000) {
    tAccum = nowMs;
    if (bpm > 0.0f && campioniSess < 65000) {
      coerenzaSomma += coerenza; bpmSomma += bpm; campioniSess++;
    }
  }

  if ((uint32_t)(nowMs - tUltimoBattito) > T_PULSE_TOUT_MS) {
    if (stato != ST_NO_PULSE) {
      uint32_t durata = nowMs - tInizioRun;
      if (durata >= SESS_MIN_MS) salvaSessione(durata);
      coerenzaSomma = 0; bpmSomma = 0; campioniSess = 0;
      bpm = 0; ibiCount = 0; ibiIdx = 0; coerenza = 0;
      aggiornaRisonanza();
    }
    stato = ST_NO_PULSE; audioAttivo = false;
  } else if (stato == ST_NO_PULSE) {
    tInizioRun = nowMs; tAccum = nowMs; stato = ST_RUN; audioAttivo = true;
  }
}


/* ===========================================================================
   14.  ATTUATORI
   =========================================================================== */

void taskAttuatori(uint32_t nowMs) {
  uint8_t r = 0, g = 0, b = 0;
  uint8_t aA = 0, aB = 0;
#if DISPOSIZIONE_IBRIDA
  uint16_t mA = 0, mB = 0;
#endif

  switch (stato) {
    case ST_CALIB: {
      uint32_t t = nowMs % 5000UL;
      uint32_t k = (t < 2500) ? (t * 255 / 2500) : ((5000 - t) * 255 / 2500);
      r = COL_CAL[0] * k / 255; g = COL_CAL[1] * k / 255; b = COL_CAL[2] * k / 255;
      break;
    }
    case ST_RUN: {
      float fr = COL_ESP[0] + ((float)COL_INSP[0] - COL_ESP[0]) * flusso;
      float fg = COL_ESP[1] + ((float)COL_INSP[1] - COL_ESP[1]) * flusso;
      float fb = COL_ESP[2] + ((float)COL_INSP[2] - COL_ESP[2]) * flusso;
      float sat = SAT_MIN + (1.0f - SAT_MIN) * coerenza;
      float gr = (fr + fg + fb) / 3.0f;
      fr = gr + (fr - gr) * sat; fg = gr + (fg - gr) * sat; fb = gr + (fb - gr) * sat;
      float lum = LUM_MIN + (1.0f - LUM_MIN) * profondita;
      r = (uint8_t)(fr * lum); g = (uint8_t)(fg * lum); b = (uint8_t)(fb * lum);

      aA = livelloTattile(profondita * flusso);
      aB = livelloTattile(profondita * (1.0f - flusso));

      /* campana al cambio di direzione */
      if (inspirazione != inspPrec) {
        inspPrec = inspirazione;
        float esc = fabsf(profondita - profAlTurno);
        profAlTurno = profondita;
        audioVelCampana = 0.25f + 0.5f * constrain(esc, 0.0f, 1.0f) + 0.25f * coerenza;
        audioCampana = inspirazione ? -1 : +1;   /* fondo grave, vertice acuta */
      }
#if DISPOSIZIONE_IBRIDA
      float amp = MOT_FONDO + (1.0f - MOT_FONDO) * profondita;
      float kA = amp * radAlto, kB = amp * radBasso;
      mA = (kA < MOT_DEADZONE) ? 0 : MOT_DUTY_MIN + (uint16_t)((MOT_DUTY_MAX - MOT_DUTY_MIN) * constrain(kA, 0.0f, 1.0f));
      mB = (kB < MOT_DEADZONE) ? 0 : MOT_DUTY_MIN + (uint16_t)((MOT_DUTY_MAX - MOT_DUTY_MIN) * constrain(kB, 0.0f, 1.0f));
#endif
      break;
    }
    case ST_NO_PULSE:
      if (((nowMs / T_BLINK_MS) & 1) == 0) { r = COL_ALERT[0]; g = COL_ALERT[1]; b = COL_ALERT[2]; }
      break;
    case ST_FSR_FAULT:
      if (((nowMs / (T_BLINK_MS * 2)) & 1) == 0) { r = COL_FAULT[0]; g = COL_FAULT[1]; b = COL_FAULT[2]; }
      break;
  }

  portENTER_CRITICAL(&muxTat);
  ampAlto = aA; ampBasso = aB;
  portEXIT_CRITICAL(&muxTat);

#if DISPOSIZIONE_IBRIDA
  ledcWrite(CH_MA, mA); ledcWrite(CH_MB, mB);
#endif
  scriviLed(r, g, b);
  digitalWrite(PIN_STATUS, ((uint32_t)(nowMs - tFlashLed) < T_FLASH_MS) ? HIGH : LOW);
}


/* ===========================================================================
   15.  SETUP
   =========================================================================== */

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_STATUS, OUTPUT);
  pinMode(PIN_SHAM, INPUT_PULLUP);
  delay(2);
#if SHAM_ABILITATO
  shamAttivo = (digitalRead(PIN_SHAM) == LOW);
#endif

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_FSR, ADC_11db);   /* fondoscala ~3,3 V          */

  ledcSetup(CH_R, LEDC_FREQ, LEDC_BIT); ledcAttachPin(PIN_LED_R, CH_R);
  ledcSetup(CH_G, LEDC_FREQ, LEDC_BIT); ledcAttachPin(PIN_LED_G, CH_G);
  ledcSetup(CH_B, LEDC_FREQ, LEDC_BIT); ledcAttachPin(PIN_LED_B, CH_B);
#if DISPOSIZIONE_IBRIDA
  ledcSetup(CH_MA, 1000, LEDC_BIT); ledcAttachPin(PIN_MOT_ALTO,  CH_MA);
  ledcSetup(CH_MB, 1000, LEDC_BIT); ledcAttachPin(PIN_MOT_BASSO, CH_MB);
#endif
  scriviLed(0, 0, 0);

  dacWrite(PIN_TAT_ALTO,  TAT_RIPOSO);   /* riposo = meta' scala, non zero    */
  dacWrite(PIN_TAT_BASSO, TAT_RIPOSO);

  costruisciTabellaSeno();

  prefs.begin("nrm", false);
#if TELEMETRIA
  stampaSessioni();
  Serial.printf("# sessione corrente: %s\n", shamAttivo ? "SHAM, nessun 40 Hz" : "attiva");
  Serial.println(F("stato,fsr,profondita,flusso,bpm,coerenza,molt,fHz"));
#endif

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);
#if !INGRESSI_SINTETICI
  if (!ppgSensore.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("# MAX30102 non risponde: controlla I2C e alimentazione 3,3 V"));
  } else {
    /* modo a un solo LED infrarosso: il rosso serve alla saturazione, non a noi */
    ppgSensore.setup(0x1F, 4, 2, 200, 411, 4096);
    ppgSensore.setPulseAmplitudeRed(0);
    ppgSensore.setPulseAmplitudeIR(0x1F);
  }
#endif

  bpm = 0; aggiornaRisonanza();

  /* timer del canale tattile: 4 kHz, cento campioni per ciclo a 40 Hz        */
  timerTat = timerBegin(0, 80, true);              /* 80 MHz / 80 = 1 MHz     */
  timerAttachInterrupt(timerTat, &isrTattile, true);
  timerAlarmWrite(timerTat, 1000000UL / TAT_SR, true);
  timerAlarmEnable(timerTat);

  avviaAudio();

  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);
}


/* ===========================================================================
   16.  LOOP  -  core 1. Il core 0 fa solo audio.
   =========================================================================== */

void loop() {
  esp_task_wdt_reset();
  uint32_t nowMs = millis();

  taskPPG(nowMs);
  taskRespiro(nowMs);
  taskStato(nowMs);
  taskAttuatori(nowMs);

#if TELEMETRIA
  if ((uint32_t)(nowMs - tTelemPrec) >= T_TELEM_MS) {
    tTelemPrec = nowMs;
    Serial.printf("%u,%d,%.3f,%.3f,%.1f,%.3f,%u,%.2f\n",
                  stato, (int)emaVeloce, profondita, flusso,
                  bpm, coerenza, moltiplic, fRisonanza);
  }
#endif
  delay(1);                    /* lascia respirare lo scheduler FreeRTOS      */
}


/* =============================================================================
   NOTE

   NOTA CORE. Scritto per arduino-esp32 2.0.x. Sulla 3.x le API LEDC e timer
   sono cambiate: ledcSetup + ledcAttachPin diventano ledcAttach, e timerBegin
   prende la frequenza invece del prescaler. Il driver i2s legacy resta
   disponibile ma deprecato. Se compili sulla 3.x, sono le tre cose da
   adattare, e sono tutte concentrate in setup().

   LIBRERIA. Serve SparkFun MAX3010x Pulse and Proximity Sensor Library,
   installabile dal gestore librerie dell'IDE. E' l'unica dipendenza esterna.

   TARATURA
   1) TAT_MAX parte da 100 su 128 e va ABBASSATO: il biofeedback funziona
      meglio con uno stimolo appena percepibile. Molti si fermano intorno a 60.
   2) PPG_AMP_MIN dipende da quanto e' buono il contatto del sensore. Guarda
      la colonna coerenza in telemetria: se resta a zero mentre il LED di
      stato lampeggia regolarmente, il problema e' la soglia, non il sensore.
   3) I volumi delle tre voci audio stanno in taskAudio come costanti
      moltiplicative (0.42, 0.22, 0.30). Il rapporto conta piu' del livello
      assoluto: alza il volume sull'amplificatore, non qui.

   USCITA TATTILE, per canale:
        GPIO25 (o 26) ──[10 kΩ]──┬──[1,5 kΩ]── GND
                                 └──[4,7 µF]── ingresso amplificatore
      Il DAC produce gia' una tensione analogica: NON serve nessun filtro
      anti-portante, che era invece obbligatorio con il PWM dell'Uno.
      Il partitore porta i ~1,1 V picco-picco del DAC a ~0,14 V, dentro la
      finestra d'ingresso del TPA3116.
============================================================================= */
