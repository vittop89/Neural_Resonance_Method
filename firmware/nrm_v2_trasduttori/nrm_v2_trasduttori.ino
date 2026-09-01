/* =============================================================================
   NEURAL RESONANCE METHOD  -  firmware v2.0
   Biofeedback respiratorio a circuito chiuso su tre canali sensoriali:
   TATTO (40 Hz reali) - LUCE (colore e saturazione) - SUONO (sintesi GM).

   Target  : Arduino Uno R3 (ATmega328P @ 16 MHz)
   Licenza : uso personale / sperimentale.  NON e' un dispositivo medico.

   -----------------------------------------------------------------------------
   COSA CAMBIA RISPETTO ALLA v1.0
   -----------------------------------------------------------------------------
   - Attuatori tattili: trasduttori a bobina mobile al posto dei motori ERM.
     I 40 Hz ora arrivano davvero al corpo. Uscita sinusoidale, non burst.
   - Timer1 a 31,4 kHz: la portante PWM finisce cosi' lontano dai 40 Hz che
     il filtro RC la cancella del tutto, lasciando una sinusoide pulita.
   - Nuovo canale SUONO: sintetizzatore VS1053 in modo MIDI hardware.
     Tre voci, tutte modulate dal respiro in tempo reale.
   - Nuovo indice di COERENZA CARDIORESPIRATORIA (RSA): misura se il battito
     accelera in inspirazione e rallenta in espirazione. Pilota la saturazione
     del colore e il volume delle campane. E' il vero anello di biofeedback:
     si chiude sulla fisiologia, non solo sul sensore di respiro.
   - Mappa pin riorganizzata: i LED liberano il pin 11, la seriale hardware
     passa al MIDI.

   -----------------------------------------------------------------------------
   MAPPA PIN
   -----------------------------------------------------------------------------
     A0   IN   FSR402 su partitore 10k ....... respiro
     A1   IN   Pulse Sensor PPG .............. battito (lobo orecchio)
     D1   OUT  TX seriale -> pin "Rx" VS1053 . MIDI a 31250 baud
                 Il breakout Adafruit 1381 ha i level shifter a bordo: si
                 collega diretto ai 5 V dell'Arduino, senza partitori.
                 I soli pin GPIO NON sono 5 V safe: GPIO-1 va sui 3,3 V.
     D8   OUT  reset VS1053 (attivo basso)
     D9   OUT  rete RC+attenuatore -> ampli L  trasduttore STERNO   [Timer1]
     D10  OUT  rete RC+attenuatore -> ampli R  trasduttore ADDOME   [Timer1]
                 NESSUN MOSFET su questi due: sono segnali, non potenza.
                 Rete per canale: 10k in serie, 1k verso massa, 1uF verso
                 massa, poi 4,7uF in serie all'ingresso dell'amplificatore.
     D3   OUT  gate MOSFET  LED ROSSO ........................... [Timer2]
     D5   OUT  gate MOSFET  LED VERDE ........................... [Timer0]
     D6   OUT  gate MOSFET  LED BLU ............................. [Timer0]
     D13  OUT  LED integrato: flash a ogni battito rilevato
     liberi: D2, D4, D7, D11, D12, A2..A5

   MAPPA PIN CON DISPOSIZIONE_IBRIDA = 1
     I tre canali LED lasciano D3/D5/D6 e passano su un espansore PCA9685:
     A4   I/O  SDA  ⎫ I2C verso il PCA9685, indirizzo 0x40
     A5   OUT  SCL  ⎭ canali 0, 1, 2 = rosso, verde, blu
     D3   OUT  gate MOSFET  motorino STERNO  .................... [Timer2]
     D5   OUT  gate MOSFET  motorino ADDOME  .................... [Timer0]
     D6        libero
     I motorini SONO carichi induttivi: serve un 1N5819 in antiparallelo su
     ciascuno, catodo al positivo. Senza, i picchi di commutazione rientrano
     nell'ADC e rovinano la lettura del battito.

   NOTA TIMER
     Timer0 -> pin 5 e 6. Governa millis(), micros(), delay().
               Il prescaler NON viene mai modificato: base tempi esatta.
     Timer1 -> pin 9 e 10. Prescaler a /1 = 31372 Hz. Serve al canale tattile.
     Timer2 -> pin 3. Lasciato al default (490 Hz): e' solo un LED.

   NOTA SERIALE
     Con MIDI attivo la seriale hardware parla a 31250 baud e la telemetria
     non e' disponibile: le due cose si escludono a vicenda. In fase di
     taratura si compila con TELEMETRIA 1 / MIDI 0, in uso con MIDI 1.
     Durante l'upload dello sketch conviene staccare il filo verso l'RX del
     VS1053: riceverebbe il flusso del bootloader (innocuo ma rumoroso).
   ============================================================================= */


/* ===========================================================================
   1.  CONFIGURAZIONE COMPILAZIONE
   =========================================================================== */

#define MIDI_ATTIVO   1   /* 1 = canale suono attivo sulla seriale hardware   */
#define TELEMETRIA    0   /* 1 = CSV a 115200. ESCLUDE MIDI_ATTIVO.           */
#define PHASE_LOCK    1   /* 1 = azzera la fase di risonanza a ogni battito   */

/* DISPOSIZIONE IBRIDA (vedi hardware/WIRING.md, disposizione C).
   0 = base: trasduttori sotto il pannello, LED su PWM nativo dell'Uno.
   1 = ibrida: in piu' due micromotori sulla fascia toracica che portano il
       "dove" (lo scivolamento sterno/addome col respiro), mentre i
       trasduttori restano al "cosa" (la portante a 40 Hz).

   Perche' serve un espansore. La disposizione ibrida vuole SETTE canali PWM
   (2 trasduttori + 3 LED + 2 motorini) e l'Uno ne ha sei; i due trasduttori
   non sono spostabili, perche' devono stare su Timer1 per arrivare a 31 kHz
   senza toccare millis(). Con IBRIDA a 1 i tre canali LED passano su un
   modulo PCA9685 via I2C, liberando D3 e D5 per i motorini.                  */
#define DISPOSIZIONE_IBRIDA  0

/* MODALITA' SHAM (controllo cieco).
   Con il ponticello D4-GND chiuso all'accensione, la portante a 40 Hz viene
   azzerata e TUTTO IL RESTO resta identico: luce, suono, scivolamento,
   calcolo della coerenza, tempi. Serve a confrontare sessioni con e senza
   stimolazione senza sapere quale si sta facendo.

   Perche' un ponticello e non un #define: un #define lo decidi tu che compili,
   quindi sai sempre in che condizione sei e il confronto non vale niente.
   Il ponticello lo puo' mettere qualcun altro, o lo si tira a sorte e si
   annota su un foglio da confrontare a fine ciclo di sessioni.
   Lo stato viene registrato in EEPROM insieme al risultato della sessione,
   e stampato solo in modalita' telemetria: durante l'uso non trapela.       */
#define SHAM_ABILITATO  1

/* INGRESSI SINTETICI.
   A 1 i due sensori vengono sostituiti da segnali generati: respiro
   sinusoidale a 0,2 Hz e battito a ~65 BPM con aritmia sinusale simulata.
   Serve per sviluppare, fare demo in classe senza volontari, e soprattutto
   per verificare la catena di rilevamento contro un segnale di cui si
   conosce la verita'. Con l'RSA simulata l'indice di coerenza deve salire
   verso 1: se non lo fa, il difetto e' nel codice, non nella fisiologia.   */
#define INGRESSI_SINTETICI  0

/* WATCHDOG hardware. Se il loop si blocca, l'ultimo valore PWM resterebbe
   latchato e i trasduttori continuerebbero a vibrare all'infinito su un
   apparecchio a contatto col corpo. Con il watchdog il micro si resetta e
   riparte da setup(), che riporta tutto a riposo.
   Richiede un bootloader recente (Optiboot, standard su Uno R3). Se la
   scheda entra in reset continuo, il bootloader e' vecchio: metti 0.       */
#define WATCHDOG_ABILITATO  1

#if MIDI_ATTIVO && TELEMETRIA
#error "MIDI_ATTIVO e TELEMETRIA usano la stessa seriale: attivane solo uno."
#endif

#include <EEPROM.h>       /* registro delle sessioni                          */
#if WATCHDOG_ABILITATO
#include <avr/wdt.h>
#endif
#if DISPOSIZIONE_IBRIDA
#include <Wire.h>         /* libreria di sistema, non esterna                 */
#endif


/* ===========================================================================
   2.  PIN
   =========================================================================== */

/* MAPPA PIN v2.1 - verificata contro le schede tecniche dei costruttori.

   La via MIDI su seriale e' CONFERMATA dalla guida ufficiale Adafruit per il
   breakout VS1053 (prodotto 1381): il modo MIDI si abilita con due ponticelli
   e i byte entrano dal pin "Rx" della scheda a 31250 baud. Non serve SPI,
   quindi D11/D12/D13 restano liberi e il LED di stato puo' stare sul D13
   integrato, senza cablare niente.

   ATTENZIONE, dalla stessa guida: i pin di interfaccia del breakout hanno i
   level shifter a bordo e sono 5 V compatibili, MA i pin GPIO no.
   GPIO-1 va al pin 3,3 V dell'Arduino, mai ai 5 V.                            */

const uint8_t PIN_FSR      = A0;  /* respiro                                  */
const uint8_t PIN_PPG      = A1;  /* battito                                  */
const uint8_t PIN_VS_RESET = 8;   /* reset VS1053, attivo basso               */
const uint8_t PIN_TAT_ALTO = 9;   /* trasduttore sterno            [Timer1]   */
const uint8_t PIN_TAT_BASSO= 10;  /* trasduttore addome            [Timer1]   */
const uint8_t PIN_STATUS   = 13;  /* LED integrato, nessun cablaggio          */
const uint8_t PIN_SHAM     = 4;   /* ponticello verso GND = sessione sham     */

#if DISPOSIZIONE_IBRIDA
/* I LED passano sui canali 0,1,2 del PCA9685; D3 e D5 vanno ai motorini.
   D6 resta libero. A4/A5 diventano SDA/SCL.                                  */
const uint8_t PIN_MOT_ALTO = 3;   /* motorino sterno               [Timer2]   */
const uint8_t PIN_MOT_BASSO= 5;   /* motorino addome               [Timer0]   */
const uint8_t CH_LED_R     = 0;   /* canale PCA9685                           */
const uint8_t CH_LED_G     = 1;
const uint8_t CH_LED_B     = 2;
#else
const uint8_t PIN_LED_R    = 3;   /* canale rosso                  [Timer2]   */
const uint8_t PIN_LED_G    = 5;   /* canale verde                  [Timer0]   */
const uint8_t PIN_LED_B    = 6;   /* canale blu                    [Timer0]   */
#endif

/* D1 (TX seriale) -> pin "Rx" del breakout VS1053, MIDI a 31250 baud.
   Liberi: D2, D4, D7, D11, D12, A2..A5.

   RIPIEGO, se al posto dell'Adafruit monti un modulo generico privo di GPIO
   esposti: si passa al MIDI su SPI con la libreria Adafruit_VS1053. Servono
   D11/D12/D13 piu' tre pin di controllo (D2 = XCS, D4 = XDCS, D7 = DREQ), e
   il LED di stato va spostato su A2. Cambiano solo il corpo di midi2() e
   midi3(): tutto il resto del firmware resta identico.                        */


/* ===========================================================================
   3.  COSTANTI DI TEMPO  (millisecondi salvo diversa indicazione)
   =========================================================================== */

const uint16_t T_CALIB_MS       = 10000; /* calibrazione automatica FSR        */
const uint16_t T_SETTLE_MS      = 500;   /* scarto iniziale                    */
const uint16_t T_PPG_US         = 2000;  /* campionamento PPG = 500 Hz         */
const uint8_t  T_RESP_MS        = 20;    /* campionamento respiro = 50 Hz      */
const uint8_t  T_AUDIO_MS       = 50;    /* aggiornamento MIDI = 20 Hz         */
const uint16_t T_TELEM_MS       = 100;   /* telemetria                         */
const uint16_t T_REFRATTARIO_MS = 500;   /* finestra refrattaria: max 120 BPM  */
const uint16_t T_IBI_MAX_MS     = 2000;  /* IBI oltre il quale scarto          */
const uint16_t T_PULSE_TOUT_MS  = 3000;  /* SICUREZZA: nessun battito -> stop  */
const uint16_t T_FSR_FAULT_MS   = 2000;  /* FSR a fondoscala -> guasto         */
const uint16_t T_BLINK_MS       = 250;   /* semiperiodo allarme (2 Hz)         */
const uint16_t T_FLASH_MS       = 60;    /* flash LED 13 sul battito           */
const uint16_t T_CAMPANA_MS     = 1600;  /* durata nota delle campane          */


/* ===========================================================================
   4.  RISONANZA GAMMA
   =========================================================================== */

const float F_GAMMA = 40.0f;  /* target onde gamma                            */
const float F_MIN   = 30.0f;  /* limite inferiore                             */
const float F_MAX   = 50.0f;  /* limite superiore                             */


/* ===========================================================================
   5.  ATTUATORI TATTILI
   ---------------------------------------------------------------------------
   Con un trasduttore a bobina mobile non esiste soglia di spunto: si parte
   da zero. Il tetto va tenuto basso, perche' il biofeedback funziona meglio
   con uno stimolo appena percepibile che con uno stimolo forte.
   =========================================================================== */

const uint8_t TAT_RIPOSO   = 128;   /* meta' scala = silenzio (vedi sotto)    */
const uint8_t TAT_MAX      = 200;   /* ampiezza picco-picco massima, su 255   */
const float   TAT_DEADZONE = 0.03f; /* sotto: uscita ferma a TAT_RIPOSO       */

/* PERCHE' L'USCITA RIPOSA A META' SCALA E NON A ZERO.
   Il PWM filtrato diventa una tensione continua: duty 0 = 0 V, duty 255 = 5 V.
   Se la sinusoide venisse generata fra 0 e "ampiezza", il valore MEDIO
   cambierebbe con il respiro, e quella deriva lentissima (0,2 Hz) arriverebbe
   all'amplificatore come una componente continua variabile: bobina spostata
   dal centro, riscaldamento, e meno escursione utile.
   Generando invece la sinusoide SIMMETRICA attorno a 128, il valore medio
   resta fisso a 2,5 V qualunque cosa faccia il respiro: cambia solo
   l'ampiezza alternata. Il condensatore d'ingresso dell'amplificatore
   toglie quel 2,5 V costante e passa solo il segnale.                         */


/* ===========================================================================
   6.  COLORI
   ---------------------------------------------------------------------------
   Freddo in inspirazione, caldo in espirazione. Non e' una scelta estetica:
   segue la fisiologia. L'inspirazione e' la fase simpatica (attivazione),
   l'espirazione quella parasimpatica (rilascio). La luce dice al corpo
   quello che il corpo sta gia' facendo.
   =========================================================================== */

const uint8_t COL_INSP [3] = {   0,  60, 255 };  /* blu freddo                 */
const uint8_t COL_ESP  [3] = { 255,  70,   0 };  /* ambra caldo                */
const uint8_t COL_CAL  [3] = {  80,   0, 160 };  /* viola: calibrazione        */
const uint8_t COL_ALERT[3] = { 255, 210,   0 };  /* giallo: battito perso      */
const uint8_t COL_FAULT[3] = { 255,   0, 120 };  /* magenta: guasto FSR        */

const float LUM_MIN = 0.20f;  /* luminosita' a polmoni vuoti                  */
const float SAT_MIN = 0.25f;  /* saturazione a coerenza zero (colore lavato)  */

/* Cadenza di aggiornamento dei LED. I colori seguono il respiro, quindi
   frazioni di hertz: 50 Hz e' gia' abbondante. Serve soprattutto in
   disposizione ibrida, dove ogni canale costa una transazione I2C da ~0,4 ms
   e scriverli a ogni giro affosserebbe la temporizzazione dei 40 Hz.         */
const uint8_t T_LED_MS = 20;


/* ===========================================================================
   5b.  MOTORINI SUL CORPO  (solo disposizione ibrida)
   ---------------------------------------------------------------------------
   Portano il "dove", non il "cosa": una rampa di intensita' con costante di
   tempo di qualche centinaio di millisecondi. Nessun bisogno di 40 Hz.
   =========================================================================== */

#if DISPOSIZIONE_IBRIDA
/* I motorini sono da 3 V alimentati dai 5 V: il duty va limitato, altrimenti
   ricevono 5 V medi e durano poco. 150/255 = 59%, cioe' circa 2,9 V medi.    */
const uint8_t MOT_DUTY_MAX = 150;
const uint8_t MOT_DUTY_MIN = 70;    /* sotto questo l'ERM non parte           */
const float   MOT_DEADZONE = 0.05f; /* sotto: motorino fermo                  */

/* Il "dove" non deve sparire a polmoni vuoti, altrimenti a fine espirazione
   non si capisce piu' dove sia la vibrazione. Questo e' il fondo che resta
   comunque acceso, sopra il quale la profondita' del respiro aggiunge.       */
const float   MOT_FONDO    = 0.35f;
#endif


/* ===========================================================================
   7.  FILTRI RESPIRO
   =========================================================================== */

const float   A_VELOCE  = 0.25f; /* alpha EMA veloce                          */
const float   A_LENTA   = 0.06f; /* alpha EMA lenta (linea di base)           */
const float   SLEW_FLOW = 0.06f; /* scivolamento fra i due trasduttori        */
const int16_t RANGE_MIN = 30;    /* escursione minima in conteggi ADC         */


/* ===========================================================================
   8.  FILTRI PPG
   =========================================================================== */

const int16_t PPG_AMP_MIN     = 12; /* ampiezza minima per considerare battito*/
const uint8_t PPG_SOGLIA_ALTA = 60; /* % ampiezza: scatto                     */
const uint8_t PPG_SOGLIA_BASSA= 40; /* % ampiezza: riarmo (isteresi)          */
const uint8_t PPG_DECADI_OGNI = 10; /* ogni N campioni l'inviluppo si stringe */
const uint8_t IBI_BUF_N       = 5;  /* media mobile su 5 intervalli R-R       */
const float   A_COERENZA      = 0.08f; /* alpha coerenza (~12 battiti)        */


/* ===========================================================================
   9.  MAPPA MIDI
   ---------------------------------------------------------------------------
   Tre voci sui primi tre canali. I numeri di programma sono GM meno uno,
   perche' il MIDI conta da zero mentre le tabelle GM contano da uno.

   ch0  DRONE     Pad 2 (warm), GM 90.  Due note tenute: E1 (41,2 Hz) ed E2.
                  E1 e' praticamente il target dei 40 Hz: il corpo sente il
                  fondamentale sulla pelle e le orecchie sentono lo stesso
                  fondamentale piu' l'ottava. Stesso tono, due sensi diversi.
   ch1  LETTO     Seashore, GM 123. Rumore di risacca, tenuto.
   ch2  CAMPANE   Tubular Bells, GM 15. Una nota al cambio di direzione.
   =========================================================================== */

const uint8_t MIDI_CH_DRONE   = 0;
const uint8_t MIDI_CH_LETTO   = 1;
const uint8_t MIDI_CH_CAMPANE = 2;

const uint8_t GM_PAD_WARM = 89;  /* GM 90  - Pad 2 (warm)                     */
const uint8_t GM_SEASHORE = 122; /* GM 123 - Seashore                         */
const uint8_t GM_BELLS    = 14;  /* GM 15  - Tubular Bells                    */

const uint8_t NOTA_DRONE_1 = 28; /* E1, 41,2 Hz - il fondamentale             */
const uint8_t NOTA_DRONE_2 = 40; /* E2, 82,4 Hz - l'ottava                    */
const uint8_t NOTA_LETTO   = 48; /* irrilevante: la risacca non ha altezza    */
const uint8_t NOTA_CAMPANA_ALTA = 76; /* E5 - vertice dell'inspirazione       */
const uint8_t NOTA_CAMPANA_BASSA= 64; /* E4 - fondo dell'espirazione          */

const uint8_t VOL_LETTO_MAX = 70;  /* il letto non deve mai coprire il drone  */
const uint8_t VOL_DRONE_MAX = 100;


/* ===========================================================================
   10.  MACCHINA A STATI
   =========================================================================== */

enum {
  ST_CALIB,      /* 0 - calibrazione automatica del range FSR                 */
  ST_RUN,        /* 1 - funzionamento normale                                 */
  ST_NO_PULSE,   /* 2 - SICUREZZA: nessun battito da > 3 s                    */
  ST_FSR_FAULT   /* 3 - fascia respiro scollegata                             */
};

uint8_t stato = ST_CALIB;


/* ===========================================================================
   11.  VARIABILI GLOBALI
   =========================================================================== */

/* --- scheduler ------------------------------------------------------------ */
uint32_t tPpgPrec   = 0;
uint32_t tRespPrec  = 0;
uint32_t tAudioPrec = 0;
uint32_t tTelemPrec = 0;
uint32_t tFasePrec  = 0;
uint32_t tShrink    = 0;

/* --- respiro -------------------------------------------------------------- */
float    emaVeloce    = 0.0f;
float    emaLenta     = 0.0f;
bool     emaInit      = false;
float    calMin       = 1023.0f;
float    calMax       = 0.0f;
float    profondita   = 0.0f;   /* 0 = polmoni vuoti, 1 = polmoni pieni       */
float    flusso       = 0.0f;   /* 0 = espirazione,   1 = inspirazione        */
bool     inspirazione = false;
#if DISPOSIZIONE_IBRIDA
float    radAlto      = 0.0f;   /* sqrt(flusso), ripartizione motorini      */
float    radBasso     = 1.0f;   /* sqrt(1-flusso)                           */
#endif

/* --- sham e registro sessione --------------------------------------------- */
bool     shamAttivo   = false;  /* letto dal ponticello una volta, in setup */
uint32_t tInizioRun   = 0;      /* millis di ingresso in ST_RUN             */
float    coerenzaSomma= 0.0f;   /* accumulatori per la media di sessione    */
float    bpmSomma     = 0.0f;
uint16_t campioniSess = 0;
uint32_t tAccum       = 0;      /* ultimo accumulo, una volta al secondo    */
bool     inspPrec     = false;  /* per rilevare il cambio di direzione        */
float    profAlTurno  = 0.5f;   /* profondita' all'ultimo cambio direzione    */
uint32_t tFsrOk       = 0;

/* --- battito -------------------------------------------------------------- */
int16_t  ppgPicco       = 512;
int16_t  ppgValle       = 512;
bool     battArmato     = false;
uint8_t  decadiCnt      = 0;
uint32_t tUltimoBattito = 0;
uint32_t tFlashLed      = 0;
uint16_t ibiBuf[IBI_BUF_N];
uint8_t  ibiIdx         = 0;
uint8_t  ibiCount       = 0;
float    bpm            = 0.0f;
float    ibiMedio       = 0.0f;
float    coerenza       = 0.0f; /* 0 = nessuna RSA, 1 = respiro e cuore in fase*/

/* --- risonanza ------------------------------------------------------------ */
uint32_t faseAcc    = 0;
uint32_t faseIncUs  = 0;
float    fRisonanza = F_GAMMA;
uint16_t moltiplic  = 0;

/* --- audio ---------------------------------------------------------------- */
uint32_t tCampanaOff = 0;       /* quando spegnere la campana in suono        */
uint8_t  campanaNota = 0;       /* nota attualmente accesa (0 = nessuna)      */
uint8_t  ccVolDrone  = 255;     /* ultimi valori inviati: evita CC ridondanti */
uint8_t  ccBriDrone  = 255;
uint8_t  ccVolLetto  = 255;
bool     audioMuto   = true;

/* --- tabella seno --------------------------------------------------------- */
uint8_t tabSeno[256];


/* ===========================================================================
   12.  UTILITA'
   =========================================================================== */

/* Correzione gamma 2.0 con sola aritmetica intera. L'occhio e' logaritmico:
   senza questa una rampa lineare di PWM sembra saltare tutta in fondo.        */
static inline uint8_t gamma2(uint8_t v) {
  return (uint8_t)(((uint16_t)v * (uint16_t)v) / 255);
}

#if DISPOSIZIONE_IBRIDA
/* --------------------------------------------------------------------------
   PCA9685: espansore PWM a 16 canali, 12 bit, su I2C.
   Driver scritto qui invece di usare una libreria esterna: sono trenta righe
   e il progetto resta compilabile con la sola IDE Arduino.
   -------------------------------------------------------------------------- */
const uint8_t PCA_ADDR      = 0x40;  /* indirizzo di fabbrica                 */
const uint8_t PCA_MODE1     = 0x00;
const uint8_t PCA_PRESCALE  = 0xFE;
const uint8_t PCA_LED0_ON_L = 0x06;

void pcaScrivi(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

/* Duty 0..255 sul canale indicato, con gestione esplicita dei due estremi.
   Il PCA9685 ha due bit dedicati "sempre acceso" e "sempre spento": usarli
   evita il micro-impulso che altrimenti resta a duty zero.                    */
void pcaDuty(uint8_t canale, uint8_t duty) {
  uint16_t on = 0, off = 0;
  if      (duty == 0)   off = 0x1000;                        /* full OFF      */
  else if (duty == 255) on  = 0x1000;                        /* full ON       */
  else                  off = (uint16_t)(((uint32_t)duty * 4096UL) / 255UL);

  Wire.beginTransmission(PCA_ADDR);
  Wire.write(PCA_LED0_ON_L + 4 * canale);
  Wire.write((uint8_t)(on  & 0xFF));  Wire.write((uint8_t)(on  >> 8));
  Wire.write((uint8_t)(off & 0xFF));  Wire.write((uint8_t)(off >> 8));
  Wire.endTransmission();
}

/* Porta il PCA9685 a ~1000 Hz. Il prescaler si puo' scrivere solo con il
   chip in sleep, poi va risvegliato e gli si lascia stabilizzare l'oscillatore. */
void pcaInit() {
  Wire.begin();
  Wire.setClock(400000);              /* I2C veloce: 0,4 ms -> 0,1 ms a canale */

  /* prescale = 25 MHz / (4096 * f) - 1, arrotondato. Per 1000 Hz da 5.       */
  const uint8_t prescale = 5;

  pcaScrivi(PCA_MODE1, 0x10);         /* SLEEP                                 */
  pcaScrivi(PCA_PRESCALE, prescale);
  pcaScrivi(PCA_MODE1, 0x00);         /* sveglia                               */
  delay(1);                           /* l'oscillatore vuole ~500 us           */
  pcaScrivi(PCA_MODE1, 0xA0);         /* RESTART + auto-increment              */

  for (uint8_t c = 0; c < 3; c++) pcaDuty(c, 0);   /* luci spente all'avvio    */
}
#endif  /* DISPOSIZIONE_IBRIDA */


/* Scrive i tre canali LED applicando la correzione gamma.
   Aggiornamento limitato a T_LED_MS e saltato se il colore non e' cambiato:
   in disposizione ibrida ogni canale e' una transazione I2C, e scriverli a
   ogni giro di loop ruberebbe tempo alla fase dei 40 Hz.                      */
void scriviLed(uint8_t r, uint8_t g, uint8_t b) {
  static uint32_t tPrec = 0;
  static uint8_t  rPrec = 0, gPrec = 0, bPrec = 0;
  static bool     primo = true;        /* la prima scrittura non aspetta       */

  if (!primo && r == rPrec && g == gPrec && b == bPrec) return;  /* nulla e' cambiato */
  uint32_t now = millis();
  if (!primo && (uint32_t)(now - tPrec) < T_LED_MS) return;      /* non e' ancora ora */
  primo = false;
  tPrec = now;
  rPrec = r; gPrec = g; bPrec = b;

#if DISPOSIZIONE_IBRIDA
  pcaDuty(CH_LED_R, gamma2(r));
  pcaDuty(CH_LED_G, gamma2(g));
  pcaDuty(CH_LED_B, gamma2(b));
#else
  analogWrite(PIN_LED_R, gamma2(r));
  analogWrite(PIN_LED_G, gamma2(g));
  analogWrite(PIN_LED_B, gamma2(b));
#endif
}

#if DISPOSIZIONE_IBRIDA
/* Scrive il duty dei due motorini sul corpo.                                  */
void scriviMotori(uint8_t alto, uint8_t basso) {
  analogWrite(PIN_MOT_ALTO,  alto);
  analogWrite(PIN_MOT_BASSO, basso);
}

/* Da intensita' richiesta 0..1 al duty, rispettando soglia di spunto e tetto. */
uint8_t livelloMotore(float k) {
  if (k < MOT_DEADZONE) return 0;
  if (k > 1.0f) k = 1.0f;
  return (uint8_t)(MOT_DUTY_MIN + (float)(MOT_DUTY_MAX - MOT_DUTY_MIN) * k);
}
#endif

/* Scrive il PWM dei due canali tattili. Nessuna gamma: e' un segnale audio.  */
void scriviTattile(uint8_t alto, uint8_t basso) {
  analogWrite(PIN_TAT_ALTO,  alto);
  analogWrite(PIN_TAT_BASSO, basso);
}

/* Ampiezza tattile: da intensita' richiesta 0..1 e fase, al duty PWM.
   Sinusoide simmetrica attorno a TAT_RIPOSO: la continua non si muove mai.    */
uint8_t livelloTattile(float k, uint8_t fase) {
  /* CONTROLLO CIECO: in sessione sham la portante non viene mai generata.
     Tutto il resto del sistema si comporta in modo identico.                  */
  if (shamAttivo)       return TAT_RIPOSO;
  if (k < TAT_DEADZONE) return TAT_RIPOSO;    /* silenzio, non zero            */
  if (k > 1.0f) k = 1.0f;

  int16_t amp = (int16_t)((float)TAT_MAX * k);          /* 0..TAT_MAX          */
  int16_t ac  = (int16_t)tabSeno[fase] - 128;           /* -128..+127          */
  int16_t out = (int16_t)TAT_RIPOSO + (int16_t)(((int32_t)ac * (int32_t)amp) / 255L);

  if (out < 0)   out = 0;                     /* saturazione di sicurezza      */
  if (out > 255) out = 255;
  return (uint8_t)out;
}

/* Costruisce la tabella del seno rialzato 0..255 su un ciclo completo.        */
void costruisciTabellaSeno() {
  for (uint16_t i = 0; i < 256; i++) {
    float a = (float)i * (2.0f * 3.14159265f / 256.0f);
    tabSeno[i] = (uint8_t)((sin(a) * 0.5f + 0.5f) * 255.0f + 0.5f);
  }
}


/* ===========================================================================
   12b.  INGRESSI: REALI O SINTETICI
   =========================================================================== */

#if INGRESSI_SINTETICI

const float TAU = 6.28318531f;
const uint32_t RESP_PERIODO_MS = 5000;   /* 0,2 Hz: respiro lento da guida    */

uint32_t tProxBattito = 0;
uint32_t tBattitoSint = 0;
uint16_t ibiSint      = 920;

/* Fase 0..1 del respiro sintetico.                                           */
float faseRespiroSint(uint32_t nowMs) {
  return (float)(nowMs % RESP_PERIODO_MS) / (float)RESP_PERIODO_MS;
}

/* Respiro: sinusoide centrata a meta' scala, con qualche LSB di rumore per
   non rendere il filtro artificialmente facile.                              */
int16_t leggiFsr(uint32_t nowMs) {
  float s = sin(faseRespiroSint(nowMs) * TAU);
  return 512 + (int16_t)(140.0f * s) + (int16_t)random(-4, 5);
}

/* Battito con ARITMIA SINUSALE RESPIRATORIA SIMULATA: l'intervallo R-R si
   accorcia quando il respiro sale. E' il banco di prova dell'indice di
   coerenza, che con questo ingresso deve salire verso 1. Se non lo fa, il
   difetto e' nel codice.                                                     */
int16_t leggiPpg(uint32_t nowMs) {
  if ((int32_t)(nowMs - tProxBattito) >= 0) {
    tBattitoSint = tProxBattito;
    /* derivata del respiro: positiva in inspirazione -> IBI piu' corto       */
    float c = cos(faseRespiroSint(nowMs) * TAU);
    ibiSint = (uint16_t)(920.0f - 90.0f * c);          /* 830..1010 ms        */
    tProxBattito = tBattitoSint + ibiSint;
  }

  float ph = (float)(nowMs - tBattitoSint) / (float)ibiSint;
  if (ph > 1.0f) ph = 1.0f;

  float a;                                              /* forma d'onda PPG   */
  if      (ph < 0.12f) a = ph / 0.12f;                              /* salita */
  else if (ph < 0.40f) a = 1.0f - 0.75f * (ph - 0.12f) / 0.28f;     /* discesa*/
  else if (ph < 0.52f) a = 0.25f + 0.15f * (1.0f - fabs(ph - 0.46f) / 0.06f);
  else                 a = 0.25f * (1.0f - (ph - 0.52f) / 0.48f);   /* coda   */
  if (a < 0.0f) a = 0.0f;

  return 480 + (int16_t)(60.0f * a) + (int16_t)random(-2, 3);
}

#else   /* ingressi reali */

int16_t leggiFsr(uint32_t nowMs) { (void)nowMs; return (int16_t)analogRead(PIN_FSR); }
int16_t leggiPpg(uint32_t nowMs) { (void)nowMs; return (int16_t)analogRead(PIN_PPG); }

#endif


/* ===========================================================================
   12c.  REGISTRO DELLE SESSIONI IN EEPROM
   ---------------------------------------------------------------------------
   L'Uno ha 1 KB di EEPROM che questo progetto non usava affatto. Quattro byte
   per sessione bastano a rendere confrontabili le sessioni sham e quelle
   vere, che e' il punto di avere una modalita' sham.

   Il record si scrive quando si toglie il sensore dal lobo, cioe' alla
   transizione RUN -> NO_PULSE: e' il gesto naturale di fine sessione.
   Si rileggono tutti compilando con TELEMETRIA a 1: vengono stampati
   all'avvio, prima che parta la calibrazione.
   =========================================================================== */

const uint16_t EE_INDICE   = 0;       /* indirizzo del contatore              */
const uint16_t EE_PRIMO    = 1;       /* primo record                         */
const uint8_t  EE_REC_LEN  = 4;
const uint8_t  EE_MAX_REC  = 200;     /* 200 * 4 + 1 = 801 byte su 1024       */
const uint32_t SESS_MIN_MS = 120000;  /* sotto due minuti non e' una sessione */

void salvaSessione(uint32_t durataMs) {
  if (campioniSess == 0) return;

  uint8_t idx = EEPROM.read(EE_INDICE);
  if (idx >= EE_MAX_REC) idx = 0;     /* copre anche l'EEPROM vergine (255)   */
  uint16_t a = EE_PRIMO + (uint16_t)idx * EE_REC_LEN;

  uint32_t min32 = durataMs / 60000UL;
  float    coer  = (coerenzaSomma / (float)campioniSess) * 100.0f;
  float    bpmM  = bpmSomma / (float)campioniSess;

  EEPROM.update(a + 0, (uint8_t)(min32 > 255 ? 255 : min32));
  EEPROM.update(a + 1, (uint8_t)(coer > 100.0f ? 100 : (coer < 0.0f ? 0 : coer)));
  EEPROM.update(a + 2, (uint8_t)(bpmM > 255.0f ? 255 : (bpmM < 0.0f ? 0 : bpmM)));
  EEPROM.update(a + 3, shamAttivo ? 1 : 0);
  EEPROM.update(EE_INDICE, idx + 1);
}

#if TELEMETRIA
void stampaSessioni() {
  uint8_t idx = EEPROM.read(EE_INDICE);
  if (idx > EE_MAX_REC) { EEPROM.update(EE_INDICE, 0); idx = 0; }
  Serial.println(F("# --- registro sessioni ---"));
  Serial.println(F("# n,minuti,coerenza%,bpm,sham"));
  for (uint8_t i = 0; i < idx; i++) {
    uint16_t a = EE_PRIMO + (uint16_t)i * EE_REC_LEN;
    Serial.print(F("# "));       Serial.print(i);              Serial.print(',');
    Serial.print(EEPROM.read(a)); Serial.print(',');
    Serial.print(EEPROM.read(a + 1)); Serial.print(',');
    Serial.print(EEPROM.read(a + 2)); Serial.print(',');
    Serial.println(EEPROM.read(a + 3));
  }
  Serial.println(F("# --- fine registro ---"));
}
#endif


/* ===========================================================================
   13.  CONFIGURAZIONE PWM
   =========================================================================== */

void configuraPWM() {
  /* Timer1 (pin 9,10) -> prescaler /1. Phase-correct 8 bit:
     f = 16 MHz / (510 * 1) = 31372 Hz.
     Il filtro RC in uscita (1 kOhm + 1 uF, taglio a ~160 Hz) attenua questa
     portante di circa 46 dB, lasciando passare i 40 Hz praticamente intatti.
     Timer1 non ha alcun ruolo in millis().                                    */
  TCCR1B = (TCCR1B & 0b11111000) | 0b001;

  /* Timer2 (pin 3) resta al default 490 Hz: pilota solo un LED.
     Timer0 (pin 5,6) NON viene toccato: regge millis(), micros(), delay().    */
}


/* ===========================================================================
   14.  STRATO MIDI
   ---------------------------------------------------------------------------
   Il VS1053 in modo MIDI hardware accetta byte MIDI grezzi a 31250 baud
   sul suo pin RX. Nessuna libreria, nessun SPI: si scrive sulla seriale.
   Il modo si abilita via strapping: GPIO0 a massa e GPIO1 a 3,3 V al reset.
   =========================================================================== */

#if MIDI_ATTIVO

/* Messaggio a 3 byte: note on/off, control change.                            */
void midi3(uint8_t stato_, uint8_t d1, uint8_t d2) {
  Serial.write(stato_); Serial.write(d1); Serial.write(d2);
}

/* Messaggio a 2 byte: program change.                                         */
void midi2(uint8_t stato_, uint8_t d1) {
  Serial.write(stato_); Serial.write(d1);
}

/* Seleziona il banco melodico e assegna un timbro a un canale.                */
void midiTimbro(uint8_t ch, uint8_t programma) {
  midi3(0xB0 | ch, 0x00, 0x00);      /* bank select MSB = banco melodico       */
  midi2(0xC0 | ch, programma);       /* program change                         */
}

void midiNoteOn (uint8_t ch, uint8_t nota, uint8_t vel) { midi3(0x90 | ch, nota, vel); }
void midiNoteOff(uint8_t ch, uint8_t nota)              { midi3(0x80 | ch, nota, 0);   }

/* Invia un control change solo se il valore e' cambiato: cosi' la seriale
   non viene saturata e la temporizzazione del loop resta stabile.             */
void midiCC(uint8_t ch, uint8_t cc, uint8_t val, uint8_t *ultimo) {
  if (val == *ultimo) return;
  *ultimo = val;
  midi3(0xB0 | ch, cc, val);
}

/* Accende le voci tenute (drone + letto) a volume zero.                       */
void midiAvvia() {
  midiTimbro(MIDI_CH_DRONE,   GM_PAD_WARM);
  midiTimbro(MIDI_CH_LETTO,   GM_SEASHORE);
  midiTimbro(MIDI_CH_CAMPANE, GM_BELLS);

  midi3(0xB0 | MIDI_CH_DRONE, 7, 0);   /* volume a zero prima di suonare       */
  midi3(0xB0 | MIDI_CH_LETTO, 7, 0);
  midi3(0xB0 | MIDI_CH_CAMPANE, 7, 90);

  midiNoteOn(MIDI_CH_DRONE, NOTA_DRONE_1, 100);
  midiNoteOn(MIDI_CH_DRONE, NOTA_DRONE_2, 80);
  midiNoteOn(MIDI_CH_LETTO, NOTA_LETTO,   100);

  ccVolDrone = 255; ccBriDrone = 255; ccVolLetto = 255;  /* forza il primo CC  */
  audioMuto = false;
}

/* Porta a zero tutti i volumi. Usata negli stati di sicurezza.                */
void midiSilenzio() {
  if (audioMuto) return;
  midiCC(MIDI_CH_DRONE, 7, 0, &ccVolDrone);
  midiCC(MIDI_CH_LETTO, 7, 0, &ccVolLetto);
  if (campanaNota) { midiNoteOff(MIDI_CH_CAMPANE, campanaNota); campanaNota = 0; }
  audioMuto = true;
}

#endif  /* MIDI_ATTIVO */


/* ===========================================================================
   15.  MOLTIPLICATORE ARMONICO
   ---------------------------------------------------------------------------
   Hz_cuore = BPM / 60. Si cerca il moltiplicatore intero M che porta
   Hz_cuore * M il piu' vicino possibile a 40 Hz, vincolato in [30, 50].
   =========================================================================== */

void aggiornaRisonanza() {
  float hz = bpm / 60.0f;
  if (hz < 0.4f) hz = 0.4f;                   /* guardia                       */

  uint16_t m = (uint16_t)((F_GAMMA / hz) + 0.5f);   /* intero piu' vicino      */
  if (m < 1) m = 1;

  float f = hz * (float)m;

  if (f < F_MIN)      { m += 1;                 f = hz * (float)m; }
  else if (f > F_MAX) { if (m > 1) { m -= 1; }  f = hz * (float)m; }

  if (f < F_MIN) f = F_MIN;
  if (f > F_MAX) f = F_MAX;

  moltiplic  = m;
  fRisonanza = f;

  /* incremento di fase per microsecondo: f * 2^32 / 1e6 = f * 4294.967296     */
  faseIncUs = (uint32_t)(fRisonanza * 4294.967296f);
}


/* ===========================================================================
   16.  INTERVALLO R-R E COERENZA CARDIORESPIRATORIA
   ---------------------------------------------------------------------------
   L'aritmia sinusale respiratoria e' un fatto fisiologico: in inspirazione
   il cuore accelera, in espirazione rallenta. Quanto marcato sia l'effetto
   dipende dal tono vagale, e migliora quando il respiro e' lento e regolare.

   Qui si confronta, a ogni battito, la direzione dell'intervallo R-R con la
   direzione del respiro. Concordanza = coerenza alta. E' l'unica misura del
   sistema che usa entrambi i sensori insieme, ed e' il vero anello chiuso:
   il dispositivo non mostra cosa stai respirando, mostra come il tuo cuore
   sta rispondendo a quello che respiri.
   =========================================================================== */

void registraIBI(uint16_t ibi) {
  /* --- coerenza: confronto prima di aggiornare la media --------------------- */
  if (ibiCount >= 3) {
    /* IBI piu' corto del medio = cuore accelerato                             */
    bool cuoreAccelera = ((float)ibi < ibiMedio);
    bool atteso = inspirazione ? cuoreAccelera : !cuoreAccelera;
    coerenza += ((atteso ? 1.0f : 0.0f) - coerenza) * A_COERENZA;
  }

  /* --- buffer circolare e media -------------------------------------------- */
  ibiBuf[ibiIdx] = ibi;
  ibiIdx = (uint8_t)((ibiIdx + 1) % IBI_BUF_N);
  if (ibiCount < IBI_BUF_N) ibiCount++;

  uint32_t somma = 0;
  for (uint8_t i = 0; i < ibiCount; i++) somma += ibiBuf[i];
  ibiMedio = (float)somma / (float)ibiCount;

  bpm = 60000.0f / ibiMedio;
  aggiornaRisonanza();
}


/* ===========================================================================
   17.  TASK PPG  -  500 Hz
   ---------------------------------------------------------------------------
   Soglia adattiva su inviluppo picco/valle, isteresi, finestra refrattaria.
   La finestra da 500 ms scarta qualunque falso picco generato dalla
   vibrazione meccanica e fissa il tetto fisiologico a 120 BPM.
   =========================================================================== */

void taskPPG(uint32_t nowUs, uint32_t nowMs) {
  if ((uint32_t)(nowUs - tPpgPrec) < T_PPG_US) return;
  tPpgPrec = nowUs;

  int16_t raw = leggiPpg(nowMs);

  /* inviluppo: sale subito, si restringe piano                                */
  if (raw > ppgPicco) ppgPicco = raw;
  if (raw < ppgValle) ppgValle = raw;
  if (++decadiCnt >= PPG_DECADI_OGNI) {
    decadiCnt = 0;
    if (ppgPicco > ppgValle + 2) { ppgPicco--; ppgValle++; }
  }

  int16_t amp    = ppgPicco - ppgValle;
  int16_t sAlta  = ppgValle + (int16_t)(((int32_t)amp * PPG_SOGLIA_ALTA)  / 100);
  int16_t sBassa = ppgValle + (int16_t)(((int32_t)amp * PPG_SOGLIA_BASSA) / 100);

  /* fronte di salita = battito                                                */
  if (!battArmato && amp >= PPG_AMP_MIN && raw > sAlta) {
    uint32_t dt = nowMs - tUltimoBattito;

    if (dt >= T_REFRATTARIO_MS) {
      if (tUltimoBattito != 0 && dt <= T_IBI_MAX_MS) registraIBI((uint16_t)dt);
      tUltimoBattito = nowMs;
      tFlashLed      = nowMs;
      battArmato     = true;
#if PHASE_LOCK
      /* Il treno a 40 Hz riparte sul picco sistolico. Essendo fRisonanza un
         multiplo intero del battito, a regime non produce discontinuita':
         corregge solo la deriva accumulata fra un battito e il successivo.    */
      faseAcc = 0;
#endif
    }
  }

  if (battArmato && raw < sBassa) battArmato = false;
}


/* ===========================================================================
   18.  TASK RESPIRO  -  50 Hz
   =========================================================================== */

void taskRespiro(uint32_t nowMs) {
  if ((uint32_t)(nowMs - tRespPrec) < T_RESP_MS) return;
  tRespPrec = nowMs;

  int16_t raw = leggiFsr(nowMs);

  /* lettura incollata a fondoscala = fascia scollegata o partitore in corto   */
  if (raw > 5 && raw < 1018) tFsrOk = nowMs;

  if (!emaInit) { emaVeloce = (float)raw; emaLenta = (float)raw; emaInit = true; }
  emaVeloce += ((float)raw - emaVeloce) * A_VELOCE;
  emaLenta  += (emaVeloce  - emaLenta ) * A_LENTA;

  /* --- calibrazione automatica dei primi 10 secondi ------------------------ */
  if (stato == ST_CALIB) {
    if (nowMs >= T_SETTLE_MS) {
      if ((float)raw < calMin) calMin = (float)raw;
      if ((float)raw > calMax) calMax = (float)raw;
    }
    return;
  }

  /* --- auto-range continuo ------------------------------------------------- */
  /* L'FSR su fascia elastica ha isteresi intorno al 10% e deriva mentre il
     tessuto si assesta. Senza questo blocco la mappatura si sfalsa in pochi
     minuti: si espande subito, si restringe di 1 LSB al secondo per lato.     */
  if (emaVeloce > calMax) calMax = emaVeloce;
  if (emaVeloce < calMin) calMin = emaVeloce;
  if ((uint32_t)(nowMs - tShrink) >= 1000) {
    tShrink = nowMs;
    if ((calMax - calMin) > (float)(RANGE_MIN + 2)) { calMax -= 1.0f; calMin += 1.0f; }
  }

  /* --- profondita' normalizzata 0..1 --------------------------------------- */
  float range = calMax - calMin;
  if (range < (float)RANGE_MIN) range = (float)RANGE_MIN;
  profondita = (emaVeloce - calMin) / range;
  if (profondita < 0.0f) profondita = 0.0f;
  if (profondita > 1.0f) profondita = 1.0f;

  /* --- direzione: incrocio EMA veloce / EMA lenta con banda morta ---------- */
  float pendenza   = emaVeloce - emaLenta;
  float bandaMorta = range / 60.0f;
  if (bandaMorta < 1.0f) bandaMorta = 1.0f;

  if      (pendenza >  bandaMorta) inspirazione = true;
  else if (pendenza < -bandaMorta) inspirazione = false;
  /* fuori soglia mantengo la direzione precedente: isteresi                   */

  /* --- scivolamento fluido fra i due trasduttori --------------------------- */
  float target = inspirazione ? 1.0f : 0.0f;
  flusso += (target - flusso) * SLEW_FLOW;

#if DISPOSIZIONE_IBRIDA
  /* Ripartizione a RADICE QUADRATA per i due motorini sul corpo.

     Due punti che vibrano vicini non danno due sensazioni distinte: il
     sistema tattile ne fonde una sola, collocata fra i due, e la sua
     posizione dipende dal rapporto delle ampiezze. E' la "sensazione
     fantasma" descritta da von Bekesy, ed e' proprio cio' che produce
     l'impressione di scivolamento lungo lo sterno.

     Perche' il punto si sposti SENZA che l'intensita' cali a meta' corsa,
     le due ampiezze devono conservare l'energia. Con ripartizione lineare
     a meta' strada si ha 0,5 e 0,5: la somma dei quadrati vale 0,5 contro
     l'1,0 degli estremi, cioe' l'energia dimezza e si sente un buco.
     Con la radice si ha 0,707 e 0,707: somma dei quadrati 1,0, costante.
     E' la stessa legge del panning a potenza costante in audio.

     Calcolate qui, a 50 Hz, e non nel loop degli attuatori: sqrt() su AVR
     costa decine di microsecondi e nel loop girerebbe migliaia di volte
     al secondo, rubando tempo alla fase dei 40 Hz.                          */
  radAlto  = sqrt(flusso);
  radBasso = sqrt(1.0f - flusso);
#endif
}


/* ===========================================================================
   19.  TASK STATO  -  watchdog e transizioni
   =========================================================================== */

void taskStato(uint32_t nowMs) {

  if (stato == ST_CALIB) {
    if (nowMs >= T_CALIB_MS) {
      /* range degenere: l'utente non ha respirato o la fascia e' troppo lasca */
      if ((calMax - calMin) < (float)RANGE_MIN) {
        float centro = (calMax + calMin) * 0.5f;
        calMin = centro - 60.0f;
        calMax = centro + 60.0f;
#if TELEMETRIA
        Serial.println(F("# escursione FSR insufficiente: range di ripiego"));
#endif
      }
      tUltimoBattito = nowMs;   /* 3 s di grazia prima del watchdog            */
      tFsrOk         = nowMs;
      tShrink        = nowMs;
      inspPrec       = inspirazione;
      profAlTurno    = profondita;
      tInizioRun     = nowMs;      /* inizio sessione                        */
      tAccum         = nowMs;
      stato          = ST_RUN;
#if MIDI_ATTIVO
      midiAvvia();              /* le voci tenute partono qui                  */
#endif
#if TELEMETRIA
      Serial.print(F("# calibrazione: min=")); Serial.print((int)calMin);
      Serial.print(F(" max="));                Serial.println((int)calMax);
#endif
    }
    return;
  }

  /* guasto sensore respiro: priorita' massima                                 */
  if ((uint32_t)(nowMs - tFsrOk) > T_FSR_FAULT_MS) { stato = ST_FSR_FAULT; return; }
  if (stato == ST_FSR_FAULT) stato = ST_RUN;

  /* Accumulo per il registro di sessione: una volta al secondo, e solo con un
     battito valido, altrimenti la media verrebbe diluita dai vuoti.          */
  if (stato == ST_RUN && (uint32_t)(nowMs - tAccum) >= 1000) {
    tAccum = nowMs;
    if (bpm > 0.0f && campioniSess < 65000) {
      coerenzaSomma += coerenza;
      bpmSomma      += bpm;
      campioniSess++;
    }
  }

  /* SICUREZZA: watchdog battito                                               */
  if ((uint32_t)(nowMs - tUltimoBattito) > T_PULSE_TOUT_MS) {
    if (stato != ST_NO_PULSE) {
      /* Togliere il sensore dal lobo e' il gesto con cui finisce la sessione:
         e' qui che il record va in EEPROM.                                   */
      uint32_t durata = nowMs - tInizioRun;
      if (durata >= SESS_MIN_MS) salvaSessione(durata);
      coerenzaSomma = 0.0f;
      bpmSomma      = 0.0f;
      campioniSess  = 0;

      bpm      = 0.0f;
      ibiCount = 0;
      ibiIdx   = 0;
      coerenza = 0.0f;
      aggiornaRisonanza();      /* torna al default 40 Hz                      */
    }
    stato = ST_NO_PULSE;
  } else if (stato == ST_NO_PULSE) {
    tInizioRun = nowMs;         /* rimesso il sensore: nuova sessione         */
    tAccum     = nowMs;
    stato      = ST_RUN;
  }
}


/* ===========================================================================
   20.  TASK AUDIO  -  20 Hz
   ---------------------------------------------------------------------------
   Tre mappature, tutte continue:

     volume drone      <- profondita' del respiro
     brillantezza      <- direzione (flusso): apre in inspirazione, chiude in
                          espirazione. E' questa che fa sembrare il suono un
                          respiro invece di una manopola del volume.
     volume letto      <- inverso della profondita'. La risacca avanza mentre
                          il drone si ritira, cosi' la sonorita' complessiva
                          resta costante e cambia solo il timbro.

   E un evento: la campana al cambio di direzione, con intensita' data
   dall'escursione dell'ultimo mezzo respiro e dalla coerenza.
   =========================================================================== */

#if MIDI_ATTIVO
void taskAudio(uint32_t nowMs) {
  if ((uint32_t)(nowMs - tAudioPrec) < T_AUDIO_MS) return;
  tAudioPrec = nowMs;

  /* negli stati di sicurezza il suono tace, come i trasduttori                */
  if (stato != ST_RUN) { midiSilenzio(); return; }
  if (audioMuto) { midiAvvia(); }

  /* --- voci tenute --------------------------------------------------------- */
  uint8_t volDrone = (uint8_t)(VOL_DRONE_MAX * (0.25f + 0.75f * profondita));
  uint8_t briDrone = (uint8_t)(30.0f + 97.0f * flusso);
  uint8_t volLetto = (uint8_t)(VOL_LETTO_MAX * (1.0f - 0.6f * profondita));

  midiCC(MIDI_CH_DRONE, 7,  volDrone, &ccVolDrone);   /* CC7  volume           */
  midiCC(MIDI_CH_DRONE, 74, briDrone, &ccBriDrone);   /* CC74 brillantezza     */
  midiCC(MIDI_CH_LETTO, 7,  volLetto, &ccVolLetto);

  /* --- evento: cambio di direzione ----------------------------------------- */
  if (inspirazione != inspPrec) {
    inspPrec = inspirazione;

    /* escursione del mezzo respiro appena concluso                            */
    float esc = profondita - profAlTurno;
    if (esc < 0.0f) esc = -esc;
    if (esc > 1.0f) esc = 1.0f;
    profAlTurno = profondita;

    /* la campana premia il respiro pieno E la coerenza cardiorespiratoria:
       piu' il cuore segue il respiro, piu' il segnale e' presente             */
    uint8_t vel = (uint8_t)(25.0f + 60.0f * esc + 40.0f * coerenza);
    if (vel > 127) vel = 127;

    if (campanaNota) midiNoteOff(MIDI_CH_CAMPANE, campanaNota);
    /* inspirazione appena iniziata = siamo al fondo del respiro = nota bassa  */
    campanaNota = inspirazione ? NOTA_CAMPANA_BASSA : NOTA_CAMPANA_ALTA;
    midiNoteOn(MIDI_CH_CAMPANE, campanaNota, vel);
    tCampanaOff = nowMs + T_CAMPANA_MS;
  }

  /* --- spegnimento programmato della campana ------------------------------- */
  if (campanaNota && (int32_t)(nowMs - tCampanaOff) >= 0) {
    midiNoteOff(MIDI_CH_CAMPANE, campanaNota);
    campanaNota = 0;
  }
}
#endif


/* ===========================================================================
   21.  TASK ATTUATORI  -  ogni giro di loop
   =========================================================================== */

void taskAttuatori(uint32_t nowUs, uint32_t nowMs) {

  /* --- avanzamento della fase di risonanza --------------------------------- */
  uint32_t dt = nowUs - tFasePrec;
  tFasePrec = nowUs;
  if (dt > 4000) dt = 4000;                   /* guardia contro stalli         */
  faseAcc += faseIncUs * dt;                  /* aritmetica mod 2^32           */
  uint8_t fase = (uint8_t)(faseAcc >> 24);

  uint8_t r = 0, g = 0, b = 0;
  /* riposo = meta' scala su entrambi i canali tattili: continua ferma,
     nessun segnale alternato, quindi silenzio dopo il condensatore d'ingresso */
  uint8_t tAlto = TAT_RIPOSO, tBasso = TAT_RIPOSO;
#if DISPOSIZIONE_IBRIDA
  /* i motorini partono fermi: solo ST_RUN li accende, quindi tutti gli stati
     di sicurezza li lasciano a zero senza bisogno di scriverlo ogni volta    */
  uint8_t mAlto = 0, mBasso = 0;
#endif

  switch (stato) {

    /* ---- calibrazione: respiro guida viola a 0,2 Hz, tatto fermo ---------- */
    case ST_CALIB: {
      uint16_t t = (uint16_t)(nowMs % 5000UL);
      uint16_t k = (t < 2500) ? (uint16_t)((uint32_t)t * 255UL / 2500UL)
                              : (uint16_t)((uint32_t)(5000 - t) * 255UL / 2500UL);
      r = (uint8_t)((uint32_t)COL_CAL[0] * k / 255UL);
      g = (uint8_t)((uint32_t)COL_CAL[1] * k / 255UL);
      b = (uint8_t)((uint32_t)COL_CAL[2] * k / 255UL);
      break;
    }

    /* ---- funzionamento normale ------------------------------------------- */
    case ST_RUN: {
      /* colore: dissolvenza ambra <-> blu su 'flusso'                          */
      float fr = (float)COL_ESP[0] + ((float)COL_INSP[0] - (float)COL_ESP[0]) * flusso;
      float fg = (float)COL_ESP[1] + ((float)COL_INSP[1] - (float)COL_ESP[1]) * flusso;
      float fb = (float)COL_ESP[2] + ((float)COL_INSP[2] - (float)COL_ESP[2]) * flusso;

      /* SATURAZIONE <- COERENZA CARDIORESPIRATORIA.
         Con il cuore fuori fase dal respiro il colore sbianca; man mano che
         la RSA si instaura il colore si satura. E' il premio visivo del
         biofeedback: il display diventa piu' bello quando stai facendo bene. */
      float sat  = SAT_MIN + (1.0f - SAT_MIN) * coerenza;
      float grigio = (fr + fg + fb) / 3.0f;
      fr = grigio + (fr - grigio) * sat;
      fg = grigio + (fg - grigio) * sat;
      fb = grigio + (fb - grigio) * sat;

      /* luminosita' <- profondita' del respiro                                */
      float lum = LUM_MIN + (1.0f - LUM_MIN) * profondita;
      r = (uint8_t)(fr * lum);
      g = (uint8_t)(fg * lum);
      b = (uint8_t)(fb * lum);

      /* tatto: sinusoide a fRisonanza, ampiezza dalla profondita',
         ripartizione fra sterno e addome da 'flusso'                          */
      tAlto  = livelloTattile(profondita * flusso,          fase);
      tBasso = livelloTattile(profondita * (1.0f - flusso), fase);

#if DISPOSIZIONE_IBRIDA
      /* Il "dove" sul torso. Ripartizione a radice quadrata (vedi
         taskRespiro) per far scivolare la sensazione fantasma senza cali
         di intensita' a meta' corsa, su un fondo che non scende mai a zero:
         a fine espirazione la vibrazione deve essere debole, non assente,
         altrimenti si perde l'informazione di posizione.                     */
      float ampMot = MOT_FONDO + (1.0f - MOT_FONDO) * profondita;
      mAlto  = livelloMotore(ampMot * radAlto);
      mBasso = livelloMotore(ampMot * radBasso);
#endif
      break;
    }

    /* ---- SICUREZZA: nessun battito ---------------------------------------- */
    case ST_NO_PULSE: {
      tAlto = TAT_RIPOSO; tBasso = TAT_RIPOSO;
      if (((nowMs / T_BLINK_MS) & 1UL) == 0UL) {
        r = COL_ALERT[0]; g = COL_ALERT[1]; b = COL_ALERT[2];
      }
      break;
    }

    /* ---- guasto fascia respiro -------------------------------------------- */
    case ST_FSR_FAULT: {
      tAlto = TAT_RIPOSO; tBasso = TAT_RIPOSO;
      if (((nowMs / (uint32_t)(T_BLINK_MS * 2)) & 1UL) == 0UL) {
        r = COL_FAULT[0]; g = COL_FAULT[1]; b = COL_FAULT[2];
      }
      break;
    }
  }

  scriviTattile(tAlto, tBasso);
#if DISPOSIZIONE_IBRIDA
  scriviMotori(mAlto, mBasso);
#endif
  scriviLed(r, g, b);

  /* LED 13: flash a ogni battito. Serve a vedere a occhio se il filtro
     anti-vibrazione sta tenendo mentre i trasduttori lavorano.                */
  digitalWrite(PIN_STATUS, ((uint32_t)(nowMs - tFlashLed) < T_FLASH_MS) ? HIGH : LOW);
}


/* ===========================================================================
   22.  TELEMETRIA  (alternativa al MIDI)
   =========================================================================== */

#if TELEMETRIA
void taskTelemetria(uint32_t nowMs) {
  if ((uint32_t)(nowMs - tTelemPrec) < T_TELEM_MS) return;
  tTelemPrec = nowMs;

  Serial.print(stato);          Serial.print(',');
  Serial.print((int)emaVeloce); Serial.print(',');
  Serial.print(profondita, 3);  Serial.print(',');
  Serial.print(flusso, 3);      Serial.print(',');
  Serial.print(bpm, 1);         Serial.print(',');
  Serial.print(coerenza, 3);    Serial.print(',');
  Serial.print(moltiplic);      Serial.print(',');
  Serial.println(fRisonanza, 2);
}
#endif


/* ===========================================================================
   23.  SETUP
   =========================================================================== */

void setup() {
#if WATCHDOG_ABILITATO
  /* Disabilitato subito: setup() contiene delay() lunghi, e un watchdog gia'
     armato al reset farebbe ripartire la scheda a ciclo continuo.            */
  MCUSR = 0;
  wdt_disable();
#endif

#if MIDI_ATTIVO
  Serial.begin(31250);                   /* velocita' standard MIDI            */
#elif TELEMETRIA
  Serial.begin(115200);
  Serial.println(F("# NEURAL RESONANCE METHOD v2.0"));
  Serial.println(F("stato,fsr,profondita,flusso,bpm,coerenza,molt,fHz"));
#endif

  pinMode(PIN_TAT_ALTO,  OUTPUT);
  pinMode(PIN_TAT_BASSO, OUTPUT);
  pinMode(PIN_STATUS,    OUTPUT);
  pinMode(PIN_VS_RESET,  OUTPUT);

  /* Ponticello sham: letto una sola volta, all'accensione. Chiuso verso GND
     significa sessione di controllo, senza portante a 40 Hz.                 */
  pinMode(PIN_SHAM, INPUT_PULLUP);
  delay(2);                              /* lascia salire il pull-up          */
#if SHAM_ABILITATO
  shamAttivo = (digitalRead(PIN_SHAM) == LOW);
#endif

#if TELEMETRIA
  stampaSessioni();                      /* registro, prima della taratura    */
  Serial.print(F("# sessione corrente: "));
  Serial.println(shamAttivo ? F("SHAM, nessun 40 Hz") : F("attiva"));
#endif

#if DISPOSIZIONE_IBRIDA
  pinMode(PIN_MOT_ALTO,  OUTPUT);
  pinMode(PIN_MOT_BASSO, OUTPUT);
  scriviMotori(0, 0);                    /* motorini fermi all'accensione      */
  pcaInit();                             /* espansore PWM per i LED            */
#else
  pinMode(PIN_LED_R,     OUTPUT);
  pinMode(PIN_LED_G,     OUTPUT);
  pinMode(PIN_LED_B,     OUTPUT);
#endif

  scriviTattile(TAT_RIPOSO, TAT_RIPOSO); /* silenzio: meta scala, non zero     */
  scriviLed(0, 0, 0);

  /* reset del sintetizzatore: il modo MIDI si aggancia allo strapping dei
     GPIO al rilascio del reset, quindi va fatto prima di scrivere byte MIDI   */
  digitalWrite(PIN_VS_RESET, LOW);
  delay(20);
  digitalWrite(PIN_VS_RESET, HIGH);
  delay(150);                            /* il chip si avvia                   */

  configuraPWM();
  costruisciTabellaSeno();

  analogRead(PIN_FSR);                   /* prime letture scartate: il S/H     */
  analogRead(PIN_PPG);                   /* dell'ADC deve assestarsi           */

  tFsrOk    = millis();
  tShrink   = millis();
  tFasePrec = micros();

  bpm = 0.0f;
  aggiornaRisonanza();                   /* default sicuro: 40,00 Hz esatti    */

#if WATCHDOG_ABILITATO
  /* Armato solo ora, a delay() finiti. Il loop gira a qualche kHz: mezzo
     secondo e' larghissimo e intercetta solo i blocchi veri.                 */
  wdt_enable(WDTO_500MS);
#endif
}


/* ===========================================================================
   24.  LOOP  -  cooperativo, nessuna delay() bloccante
   =========================================================================== */

void loop() {
#if WATCHDOG_ABILITATO
  wdt_reset();
#endif
  uint32_t nowUs = micros();
  uint32_t nowMs = millis();

  taskPPG(nowUs, nowMs);                 /* 500 Hz - battito                   */
  taskRespiro(nowMs);                    /*  50 Hz - respiro e calibrazione    */
  taskStato(nowMs);                      /*  ogni giro - watchdog e stati      */
  taskAttuatori(nowUs, nowMs);           /*  ogni giro - fase 40 Hz, tatto, luce*/

#if MIDI_ATTIVO
  taskAudio(nowMs);                      /*  20 Hz - suono                     */
#endif
#if TELEMETRIA
  taskTelemetria(nowMs);                 /*  10 Hz - CSV                       */
#endif
}


/* =============================================================================
   NOTE DI TARATURA

   1) TAT_MAX: parti da 200 e SCENDI. Il biofeedback funziona meglio con uno
      stimolo appena percepibile: se lo senti chiaramente senza cercarlo, e'
      gia' troppo forte. Molti si fermano intorno a 120.

   2) Prima accensione: compila con TELEMETRIA 1 e MIDI_ATTIVO 0, tara
      respiro e battito sul Serial Plotter, poi inverti i due define.

   3) La colonna 'coerenza' in telemetria parte da 0 e sale nell'arco di
      una decina di battiti. Se resta bassa anche respirando lentamente,
      il rilevamento del battito e' rumoroso: guarda il LED 13 prima di
      dare la colpa alla fisiologia.

   4) Se il drone e' troppo presente, abbassa VOL_DRONE_MAX prima di toccare
      il volume dell'amplificatore: cosi' il rapporto fra le voci resta quello
      progettato.

   7) DISPOSIZIONE IBRIDA. Tara MOT_DUTY_MAX guardando il consumo: i motorini
      sono da 3 V e su 5 V il duty massimo sicuro sta intorno a 150/255.
      MOT_FONDO decide quanta vibrazione resta a polmoni vuoti: a 0 il "dove"
      sparisce a fine espirazione, a 0,5 non si distingue piu' il respiro.
      Lo 0,35 di default e' un compromesso da provare addosso.

   5) Catena di uscita tattile, per canale:

        pin PWM ──[10k]──┬──[1k]── GND      <- attenuatore 1:11
                         ├──[1uF]── GND     <- filtro anti-portante
                         └──[4,7uF]── ingresso ampli   <- blocco continua

      Perche' l'attenuatore. Un TPA3116 ha guadagno fisso intorno a 32 dB e
      va in saturazione con circa 0,19 V efficaci in ingresso. Il PWM di
      Arduino, filtrato, darebbe 1,4 V efficaci: sette volte troppo, e
      l'amplificatore tosa il segnale in modo permanente. Il partitore
      10k/1k porta il livello a ~0,13 V efficaci con TAT_MAX = 200: dentro
      la finestra utile, con un po' di margine sopra.

      Perche' quei valori. La resistenza equivalente vista dal condensatore
      e' 10k in parallelo a 1k, cioe' 909 ohm; con 1 uF il taglio cade a
      175 Hz. A 31 kHz l'attenuazione supera i 45 dB, quindi della portante
      non resta niente; a 40 Hz la rete e' invece piatta.

   6) Se il tuo modulo amplificatore ha gia' i condensatori d'ingresso in
      serie (quasi tutti li hanno), il 4,7 uF e' ridondante ma innocuo.
      Se NON li ha, e' l'unica cosa che impedisce a 0,27 V continui di
      essere amplificati fino a mandare corrente continua nella bobina.
      Nel dubbio, mettilo.
============================================================================= */
