/* =============================================================================
   NEURAL RESONANCE METHOD  -  firmware v1.0
   Biofeedback respiratorio + fototerapia interattiva a circuito chiuso.

   Target  : Arduino Uno R3 (ATmega328P @ 16 MHz)
   Licenza : uso personale / sperimentale.  NON e' un dispositivo medico.

   -----------------------------------------------------------------------------
   MAPPA PIN  (solo PWM hardware nativi dell'Uno: 3, 5, 6, 9, 10, 11)
   -----------------------------------------------------------------------------
     A0   IN   FSR402 su partitore 10k  ....... respiro (escursione toracica)
     A1   IN   Pulse Sensor PPG (filo viola) ... battito (lobo orecchio)
     D9   OUT  gate MOSFET  MOTORE ALTO  (sterno)   [Timer1]
     D10  OUT  gate MOSFET  MOTORE BASSO (addome)   [Timer1]
     D3   OUT  gate MOSFET  LED ROSSO               [Timer2]
     D11  OUT  gate MOSFET  LED BLU                 [Timer2]
     D6   OUT  gate MOSFET  LED VERDE               [Timer0]
     D13  OUT  LED integrato: lampeggia ad ogni battito rilevato (diagnostica)
     D5        LIBERO (Timer0, riservato a espansioni)

   NOTA TIMER  (requisito esplicito del progetto)
     Timer0 -> pin 5 e 6.  Governa millis(), micros(), delay().
               Il suo PRESCALER NON VIENE MAI MODIFICATO da questo sketch:
               tutta la base tempi resta esatta. Usare analogWrite() sui pin
               5/6 e' sicuro, perche' cambia solo il duty (OCR0A/OCR0B),
               non la frequenza di conteggio.
     Timer1 -> pin 9 e 10   (motori)  : prescaler portato a /8  = ~3.92 kHz
     Timer2 -> pin 3 e 11   (LED R/B) : prescaler portato a /8  = ~3.92 kHz
               Alzare questi due timer NON tocca millis(). Serve per avere
               ~98 cicli di portante PWM dentro ogni periodo da 40 Hz, cosi'
               l'inviluppo di risonanza e' pulito e i MOSFET non fischiano.
     Il canale VERDE resta su Timer0 a 976 Hz: differenza invisibile all'occhio
     (si vede solo con una camera ad alta velocita').
   ============================================================================= */


/* ===========================================================================
   1.  CONFIGURAZIONE COMPILAZIONE
   =========================================================================== */

#define TELEMETRIA        1   /* 1 = stampa CSV su seriale 115200 (Serial Plotter) */
#define MODO_SINUSOIDALE  0   /* 0 = burst quadro (default), 1 = inviluppo sin()   */
#define PHASE_LOCK        1   /* 1 = azzera la fase di risonanza ad ogni battito   */
#define PWM_VELOCE        1   /* 1 = alza Timer1/Timer2 a ~3.92 kHz (vedi sopra)   */


/* ===========================================================================
   2.  PIN
   =========================================================================== */

const uint8_t PIN_FSR       = A0;   /* ingresso analogico respiro               */
const uint8_t PIN_PPG       = A1;   /* ingresso analogico battito               */
const uint8_t PIN_MOT_ALTO  = 9;    /* motore sterno   (inspirazione)           */
const uint8_t PIN_MOT_BASSO = 10;   /* motore addome   (espirazione)            */
const uint8_t PIN_LED_R     = 3;    /* canale rosso striscia LED                */
const uint8_t PIN_LED_G     = 6;    /* canale verde striscia LED                */
const uint8_t PIN_LED_B     = 11;   /* canale blu striscia LED                  */
const uint8_t PIN_STATUS    = 13;   /* LED di bordo: flash ad ogni battito      */


/* ===========================================================================
   3.  COSTANTI DI TEMPO  (millisecondi salvo diversa indicazione)
   =========================================================================== */

const uint16_t T_CALIB_MS       = 10000; /* durata calibrazione automatica FSR   */
const uint16_t T_SETTLE_MS      = 500;   /* scarto iniziale: la fascia si assesta*/
const uint16_t T_PPG_US         = 2000;  /* periodo campionamento PPG = 500 Hz   */
const uint8_t  T_RESP_MS        = 20;    /* periodo campionamento respiro = 50 Hz*/
const uint16_t T_TELEM_MS       = 100;   /* cadenza stampa telemetria            */
const uint16_t T_REFRATTARIO_MS = 500;   /* FILTRO ANTI-VIBRAZIONE: tetto 120 BPM*/
const uint16_t T_IBI_MAX_MS     = 2000;  /* IBI oltre il quale scarto (< 30 BPM) */
const uint16_t T_PULSE_TOUT_MS  = 3000;  /* SICUREZZA: nessun battito -> stop    */
const uint16_t T_FSR_FAULT_MS   = 2000;  /* FSR a fondoscala fisso -> guasto     */
const uint16_t T_BLINK_MS       = 250;   /* semiperiodo lampeggio allarme (2 Hz) */
const uint16_t T_FLASH_MS       = 60;    /* durata flash LED 13 sul battito      */


/* ===========================================================================
   4.  COSTANTI DI RISONANZA GAMMA
   =========================================================================== */

const float F_GAMMA = 40.0f;  /* target onde gamma                              */
const float F_MIN   = 30.0f;  /* limite inferiore di sicurezza per i motori     */
const float F_MAX   = 50.0f;  /* limite superiore                               */

/* Frazione del ciclo in cui il motore e' ON nel modo "burst quadro".
   128/255 = 50%. Alzare a 180 se gli ERM non partono (vedi note in fondo).      */
const uint8_t GATE_SOGLIA = 128;


/* ===========================================================================
   5.  COSTANTI ATTUATORI
   =========================================================================== */

/* Duty minimo sotto il quale un motore a massa eccentrica non inizia a girare.
   VALORE DA TARARE AL BANCO: partire da 110 e scendere finche' il motore parte. */
const uint8_t MOT_MIN      = 110;
const uint8_t MOT_MAX      = 255;   /* duty massimo                              */
const float   MOT_DEADZONE = 0.03f; /* sotto questa quota il motore resta spento */

/* Colori (valori PRE-correzione gamma, 0..255 per canale) */
const uint8_t COL_INSP [3] = {   0,  60, 255 };  /* inspirazione: blu freddo     */
const uint8_t COL_ESP  [3] = { 255,  70,   0 };  /* espirazione : rosso/ambra    */
const uint8_t COL_CAL  [3] = {  80,   0, 160 };  /* calibrazione: viola pulsante */
const uint8_t COL_ALERT[3] = { 255, 210,   0 };  /* allarme battito: giallo      */
const uint8_t COL_FAULT[3] = { 255,   0, 120 };  /* guasto FSR: magenta          */

const float LUM_MIN = 0.20f;  /* luminosita' a polmoni vuoti (non spegne mai)    */


/* ===========================================================================
   6.  COSTANTI FILTRI RESPIRO
   =========================================================================== */

const float   A_VELOCE  = 0.25f; /* alpha EMA veloce  (~ costante di tempo 60 ms)*/
const float   A_LENTA   = 0.06f; /* alpha EMA lenta   (~ costante di tempo 300ms)*/
const float   SLEW_FLOW = 0.06f; /* velocita' di scivolamento tra i due motori   */
const int16_t RANGE_MIN = 30;    /* escursione minima accettabile in conteggi ADC*/


/* ===========================================================================
   7.  COSTANTI FILTRI PPG
   =========================================================================== */

const int16_t PPG_AMP_MIN     = 12; /* ampiezza picco-valle sotto la quale il    */
                                    /* segnale e' rumore e non battito           */
const uint8_t PPG_SOGLIA_ALTA = 60; /* % dell'ampiezza: soglia di scatto         */
const uint8_t PPG_SOGLIA_BASSA= 40; /* % dell'ampiezza: soglia di riarmo         */
const uint8_t PPG_DECADI_OGNI = 10; /* ogni N campioni l'inviluppo si restringe  */
const uint8_t IBI_BUF_N       = 5;  /* media mobile su 5 intervalli R-R          */


/* ===========================================================================
   8.  MACCHINA A STATI
   =========================================================================== */

enum {
  ST_CALIB,      /* 0 - primi 10 s: calibrazione automatica del range FSR       */
  ST_RUN,        /* 1 - funzionamento normale                                   */
  ST_NO_PULSE,   /* 2 - SICUREZZA: nessun battito da > 3 s, motori fermi        */
  ST_FSR_FAULT   /* 3 - fascia respiro scollegata o a fondoscala                */
};

uint8_t stato = ST_CALIB;   /* si parte sempre dalla calibrazione                */


/* ===========================================================================
   9.  VARIABILI GLOBALI
   =========================================================================== */

/* --- scheduler non bloccante --------------------------------------------- */
uint32_t tPpgPrec   = 0;    /* micros dell'ultimo campione PPG                  */
uint32_t tRespPrec  = 0;    /* millis dell'ultimo campione respiro              */
uint32_t tTelemPrec = 0;    /* millis dell'ultima stampa telemetria             */
uint32_t tFasePrec  = 0;    /* micros dell'ultimo avanzamento di fase           */
uint32_t tShrink    = 0;    /* millis dell'ultima contrazione dell'auto-range    */

/* --- respiro -------------------------------------------------------------- */
float    emaVeloce    = 0.0f;   /* media mobile veloce del segnale FSR          */
float    emaLenta     = 0.0f;   /* media mobile lenta: fa da linea di base      */
bool     emaInit      = false;  /* true dopo la prima lettura                   */
float    calMin       = 1023.0f;/* minimo rilevato in calibrazione              */
float    calMax       = 0.0f;   /* massimo rilevato in calibrazione             */
float    profondita   = 0.0f;   /* 0.0 polmoni vuoti  ->  1.0 polmoni pieni     */
float    flusso       = 0.0f;   /* 0.0 espirazione    ->  1.0 inspirazione      */
bool     inspirazione = false;  /* direzione corrente rilevata                  */
uint32_t tFsrOk       = 0;      /* millis dell'ultima lettura FSR plausibile    */

/* --- battito -------------------------------------------------------------- */
int16_t  ppgPicco       = 512;  /* inviluppo superiore adattivo                 */
int16_t  ppgValle       = 512;  /* inviluppo inferiore adattivo                 */
bool     battArmato     = false;/* true tra il fronte di salita e il riarmo     */
uint8_t  decadiCnt      = 0;    /* contatore per il decadimento dell'inviluppo  */
uint32_t tUltimoBattito = 0;    /* millis dell'ultimo battito valido (watchdog) */
uint32_t tFlashLed      = 0;    /* millis dell'ultimo flash del LED 13          */
uint16_t ibiBuf[IBI_BUF_N];     /* buffer circolare degli intervalli R-R        */
uint8_t  ibiIdx         = 0;    /* indice di scrittura nel buffer               */
uint8_t  ibiCount       = 0;    /* quanti campioni validi contiene il buffer    */
float    bpm            = 0.0f; /* battiti per minuto filtrati                  */

/* --- risonanza ------------------------------------------------------------ */
uint32_t faseAcc    = 0;        /* accumulatore di fase a 32 bit (1 giro = 2^32)*/
uint32_t faseIncUs  = 0;        /* incremento di fase per ogni microsecondo     */
float    fRisonanza = F_GAMMA;  /* frequenza effettiva applicata ai motori (Hz) */
uint16_t moltiplic  = 0;        /* moltiplicatore armonico intero corrente      */

#if MODO_SINUSOIDALE
uint8_t tabSeno[256];           /* seno rialzato 0..255 (costruito in setup)    */
#endif


/* ===========================================================================
   10.  UTILITA'
   =========================================================================== */

/* Correzione gamma 2.0 esatta con sola aritmetica intera.
   Serve perche' l'occhio e' logaritmico: senza questa, una rampa lineare di PWM
   sembra "saltare" tutta in fondo alla corsa e la dissolvenza non e' fluida.    */
static inline uint8_t gamma2(uint8_t v) {
  return (uint8_t)(((uint16_t)v * (uint16_t)v) / 255);
}

/* Scrive i tre canali della striscia LED applicando la correzione gamma.       */
void scriviLed(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(PIN_LED_R, gamma2(r));
  analogWrite(PIN_LED_G, gamma2(g));
  analogWrite(PIN_LED_B, gamma2(b));
}

/* Scrive il duty PWM dei due motori. Nessuna gamma: la risposta e' meccanica.  */
void scriviMotori(uint8_t alto, uint8_t basso) {
  analogWrite(PIN_MOT_ALTO,  alto);
  analogWrite(PIN_MOT_BASSO, basso);
}

/* Inviluppo di risonanza: dato l'angolo di fase 0..255 restituisce 0..255.     */
static inline uint8_t inviluppo(uint8_t fase) {
#if MODO_SINUSOIDALE
  return tabSeno[fase];                       /* modulazione di ampiezza dolce  */
#else
  return (fase < GATE_SOGLIA) ? 255 : 0;      /* burst quadro on/off            */
#endif
}

/* Converte l'intensita' richiesta (0..1) + l'inviluppo nel duty finale.        */
uint8_t livelloMotore(float k, uint8_t env) {
  if (k < MOT_DEADZONE) return 0;             /* sotto soglia: motore fermo     */
  if (k > 1.0f) k = 1.0f;                     /* saturazione                    */
  uint16_t amp = MOT_MIN + (uint16_t)((float)(MOT_MAX - MOT_MIN) * k);
  return (uint8_t)(((uint32_t)amp * (uint32_t)env) / 255UL);
}


/* ===========================================================================
   11.  CONFIGURAZIONE PWM
   =========================================================================== */

void configuraPWM() {
#if PWM_VELOCE
  /* Timer1 (pin 9,10) -> prescaler /8. Phase-correct 8 bit:
     f = 16 MHz / (510 * 8) = 3921.6 Hz. Non tocca millis().                    */
  TCCR1B = (TCCR1B & 0b11111000) | 0b010;

  /* Timer2 (pin 3,11) -> prescaler /8. Stessa frequenza. Non tocca millis().
     Nota: dopo questa riga tone() sarebbe sfasata, ma qui non viene usata.     */
  TCCR2B = (TCCR2B & 0b11111000) | 0b010;

  /* Timer0 (pin 5,6) NON viene toccato di proposito: e' la base tempi di
     millis(), micros() e delay(). Il canale verde restera' a ~976 Hz.          */
#endif
}


/* ===========================================================================
   12.  CALCOLO DEL MOLTIPLICATORE ARMONICO
   ---------------------------------------------------------------------------
   Hz_cuore = BPM / 60.  Si cerca il moltiplicatore intero M che porta
   Hz_cuore * M il piu' vicino possibile a 40 Hz, poi si vincola il risultato
   nella banda [30, 50] Hz. Il risultato aggiorna l'incremento di fase.
   =========================================================================== */

void aggiornaRisonanza() {
  float hz = bpm / 60.0f;                     /* frequenza cardiaca in Hz       */
  if (hz < 0.4f) hz = 0.4f;                   /* guardia: evita divisioni assurde*/

  /* moltiplicatore intero piu' vicino: round(40 / hz)                          */
  uint16_t m = (uint16_t)((F_GAMMA / hz) + 0.5f);
  if (m < 1) m = 1;

  float f = hz * (float)m;                    /* armonica risultante            */

  /* se l'arrotondamento e' finito fuori banda, correggo di un gradino          */
  if (f < F_MIN)      { m += 1;                 f = hz * (float)m; }
  else if (f > F_MAX) { if (m > 1) { m -= 1; }  f = hz * (float)m; }

  /* vincolo finale duro: i motori non escono mai da [30, 50] Hz                */
  if (f < F_MIN) f = F_MIN;
  if (f > F_MAX) f = F_MAX;

  moltiplic  = m;
  fRisonanza = f;

  /* Incremento di fase per microsecondo.
     Un giro completo = 2^32,  inc = f * 2^32 / 1e6 = f * 4294.967296           */
  faseIncUs = (uint32_t)(fRisonanza * 4294.967296f);
}


/* ===========================================================================
   13.  REGISTRAZIONE DI UN INTERVALLO BATTITO-BATTITO
   =========================================================================== */

void registraIBI(uint16_t ibi) {
  ibiBuf[ibiIdx] = ibi;                       /* scrivo nel buffer circolare    */
  ibiIdx = (uint8_t)((ibiIdx + 1) % IBI_BUF_N);
  if (ibiCount < IBI_BUF_N) ibiCount++;       /* saturo il contatore            */

  uint32_t somma = 0;                         /* media degli IBI memorizzati    */
  for (uint8_t i = 0; i < ibiCount; i++) somma += ibiBuf[i];
  float medio = (float)somma / (float)ibiCount;

  bpm = 60000.0f / medio;                     /* IBI medio -> BPM               */
  aggiornaRisonanza();                        /* ricalcolo subito l'armonica    */
}


/* ===========================================================================
   14.  TASK PPG  -  rilevamento battito a 500 Hz
   ---------------------------------------------------------------------------
   Soglia adattiva su inviluppo picco/valle + isteresi + finestra refrattaria.
   La finestra refrattaria da 500 ms e' il filtro anti-vibrazione richiesto:
   qualunque falso picco generato dai motori entro 500 ms viene ignorato,
   il che fissa anche il tetto fisiologico a 120 BPM.
   =========================================================================== */

void taskPPG(uint32_t nowUs, uint32_t nowMs) {
  if ((uint32_t)(nowUs - tPpgPrec) < T_PPG_US) return;   /* non e' ancora ora   */
  tPpgPrec = nowUs;

  int16_t raw = (int16_t)analogRead(PIN_PPG); /* campione grezzo 0..1023        */

  /* --- inseguimento dell'inviluppo ---------------------------------------- */
  if (raw > ppgPicco) ppgPicco = raw;         /* il picco sale istantaneamente  */
  if (raw < ppgValle) ppgValle = raw;         /* la valle scende istantaneamente*/

  /* Ogni PPG_DECADI_OGNI campioni (20 ms) l'inviluppo si restringe di 1 LSB per
     lato: insegue lentamente il calo di ampiezza se il sensore si sposta, senza
     inseguire il rumore rapido.                                                */
  if (++decadiCnt >= PPG_DECADI_OGNI) {
    decadiCnt = 0;
    if (ppgPicco > ppgValle + 2) { ppgPicco--; ppgValle++; }
  }

  int16_t amp = ppgPicco - ppgValle;          /* ampiezza corrente del segnale  */

  /* --- soglie di scatto e riarmo ------------------------------------------ */
  int16_t sAlta  = ppgValle + (int16_t)(((int32_t)amp * PPG_SOGLIA_ALTA)  / 100);
  int16_t sBassa = ppgValle + (int16_t)(((int32_t)amp * PPG_SOGLIA_BASSA) / 100);

  /* --- fronte di salita = battito ----------------------------------------- */
  if (!battArmato && amp >= PPG_AMP_MIN && raw > sAlta) {
    uint32_t dt = nowMs - tUltimoBattito;     /* tempo dal battito precedente   */

    if (dt >= T_REFRATTARIO_MS) {             /* FILTRO: piu' vecchio di 500 ms */
      /* uso l'intervallo per il calcolo BPM solo se e' fisiologico             */
      if (tUltimoBattito != 0 && dt <= T_IBI_MAX_MS) {
        registraIBI((uint16_t)dt);
      }
      tUltimoBattito = nowMs;                 /* alimento anche il watchdog     */
      tFlashLed      = nowMs;                 /* accendo il LED diagnostico     */
      battArmato     = true;                  /* blocco fino al riarmo          */
#if PHASE_LOCK
      /* Sincronizzazione armonica vera: il treno di burst riparte esattamente
         sul picco sistolico. Essendo fRisonanza un multiplo intero della
         frequenza cardiaca, a regime questo azzeramento non produce salti.     */
      faseAcc = 0;
#endif
    }
  }

  /* --- riarmo con isteresi ------------------------------------------------ */
  if (battArmato && raw < sBassa) battArmato = false;
}


/* ===========================================================================
   15.  TASK RESPIRO  -  50 Hz
   =========================================================================== */

void taskRespiro(uint32_t nowMs) {
  if ((uint32_t)(nowMs - tRespPrec) < T_RESP_MS) return;
  tRespPrec = nowMs;

  int16_t raw = (int16_t)analogRead(PIN_FSR); /* campione grezzo 0..1023        */

  /* --- diagnostica sensore ------------------------------------------------ */
  /* Se la lettura resta incollata a fondoscala, la fascia e' scollegata o il
     partitore e' in corto: memorizzo l'ultimo istante "plausibile".            */
  if (raw > 5 && raw < 1018) tFsrOk = nowMs;

  /* --- filtro a doppia media mobile --------------------------------------- */
  if (!emaInit) { emaVeloce = (float)raw; emaLenta = (float)raw; emaInit = true; }
  emaVeloce += ((float)raw - emaVeloce) * A_VELOCE;   /* segnale filtrato       */
  emaLenta  += (emaVeloce  - emaLenta ) * A_LENTA;    /* linea di base lenta    */

  /* --- FASE A: calibrazione automatica dei primi 10 secondi ---------------- */
  if (stato == ST_CALIB) {
    if (nowMs >= T_SETTLE_MS) {               /* scarto il transitorio iniziale */
      if ((float)raw < calMin) calMin = (float)raw;  /* min = fine espirazione  */
      if ((float)raw > calMax) calMax = (float)raw;  /* max = fine inspirazione */
    }
    return;                                   /* in calibrazione basta cosi'    */
  }

  /* --- auto-range continuo post-calibrazione ------------------------------ */
  /* L'FSR su fascia elastica deriva parecchio (isteresi ~10%, assestamento del
     tessuto). Senza questo blocco la mappatura si sfalsa dopo pochi minuti.    */
  if (emaVeloce > calMax) calMax = emaVeloce; /* espansione immediata verso alto*/
  if (emaVeloce < calMin) calMin = emaVeloce; /* espansione immediata verso basso*/
  if ((uint32_t)(nowMs - tShrink) >= 1000) {  /* contrazione lenta 1 LSB/s/lato */
    tShrink = nowMs;
    if ((calMax - calMin) > (float)(RANGE_MIN + 2)) { calMax -= 1.0f; calMin += 1.0f; }
  }

  /* --- FASE B1: profondita' respiratoria normalizzata 0..1 ----------------- */
  float range = calMax - calMin;
  if (range < (float)RANGE_MIN) range = (float)RANGE_MIN;
  profondita = (emaVeloce - calMin) / range;
  if (profondita < 0.0f) profondita = 0.0f;
  if (profondita > 1.0f) profondita = 1.0f;

  /* --- FASE B2: direzione (inspirazione / espirazione) --------------------- */
  /* Incrocio EMA veloce vs EMA lenta = derivata robusta. La banda morta evita
     che il rumore faccia sfarfallare la direzione decine di volte al secondo.  */
  float pendenza   = emaVeloce - emaLenta;
  float bandaMorta = range / 60.0f;
  if (bandaMorta < 1.0f) bandaMorta = 1.0f;

  if      (pendenza >  bandaMorta) inspirazione = true;
  else if (pendenza < -bandaMorta) inspirazione = false;
  /* fuori dalle soglie mantengo la direzione precedente (isteresi)             */

  /* --- FASE B3: scivolamento fluido tra i due motori ----------------------- */
  /* 'flusso' non salta: insegue il target con una rampa esponenziale, cosi' la
     vibrazione si sposta dallo sterno all'addome in modo continuo.             */
  float target = inspirazione ? 1.0f : 0.0f;
  flusso += (target - flusso) * SLEW_FLOW;
}


/* ===========================================================================
   16.  TASK STATO  -  watchdog e transizioni
   =========================================================================== */

void taskStato(uint32_t nowMs) {

  /* --- uscita dalla calibrazione ------------------------------------------ */
  if (stato == ST_CALIB) {
    if (nowMs >= T_CALIB_MS) {
      /* Se l'utente non ha respirato o la fascia e' troppo lasca il range e'
         degenere: uso un intervallo di ripiego centrato sulla lettura media.   */
      if ((calMax - calMin) < (float)RANGE_MIN) {
        float centro = (calMax + calMin) * 0.5f;
        calMin = centro - 60.0f;
        calMax = centro + 60.0f;
#if TELEMETRIA
        Serial.println(F("# ATTENZIONE: escursione FSR insufficiente, uso range di ripiego"));
#endif
      }
      tUltimoBattito = nowMs;   /* concedo 3 s di grazia prima del watchdog     */
      tFsrOk         = nowMs;
      tShrink        = nowMs;
      stato          = ST_RUN;
#if TELEMETRIA
      Serial.print(F("# calibrazione completata: min=")); Serial.print((int)calMin);
      Serial.print(F(" max="));                           Serial.println((int)calMax);
#endif
    }
    return;
  }

  /* --- guasto sensore respiro (priorita' massima) -------------------------- */
  if ((uint32_t)(nowMs - tFsrOk) > T_FSR_FAULT_MS) {
    stato = ST_FSR_FAULT;
    return;
  }
  if (stato == ST_FSR_FAULT) stato = ST_RUN;  /* rientro appena torna plausibile*/

  /* --- FASE D: SICUREZZA, watchdog battito -------------------------------- */
  if ((uint32_t)(nowMs - tUltimoBattito) > T_PULSE_TOUT_MS) {
    if (stato != ST_NO_PULSE) {               /* alla transizione invalido i dati*/
      bpm      = 0.0f;
      ibiCount = 0;
      ibiIdx   = 0;
      aggiornaRisonanza();                    /* torna al default 40 Hz         */
    }
    stato = ST_NO_PULSE;
  } else if (stato == ST_NO_PULSE) {
    stato = ST_RUN;                           /* battito tornato: si riparte    */
  }
}


/* ===========================================================================
   17.  TASK ATTUATORI  -  ad ogni giro di loop
   =========================================================================== */

void taskAttuatori(uint32_t nowUs, uint32_t nowMs) {

  /* --- avanzamento dell'accumulatore di fase ------------------------------ */
  uint32_t dt = nowUs - tFasePrec;            /* delta in microsecondi          */
  tFasePrec = nowUs;
  if (dt > 4000) dt = 4000;                   /* guardia contro stalli del loop */
  faseAcc += faseIncUs * dt;                  /* aritmetica mod 2^32 = mod 1 giro*/
  uint8_t fase = (uint8_t)(faseAcc >> 24);    /* 0..255 dentro il ciclo         */

  uint8_t r = 0, g = 0, b = 0;                /* uscite colore                  */
  uint8_t mAlto = 0, mBasso = 0;              /* uscite motori                  */

  switch (stato) {

    /* ---- calibrazione: respiro guida viola, motori fermi ------------------ */
    case ST_CALIB: {
      /* onda triangolare a 0.2 Hz (5 s per ciclo): l'utente respira seguendola */
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
      /* colore: dissolvenza continua ambra <-> blu pilotata da 'flusso',
         luminosita' complessiva pilotata dalla profondita' del respiro         */
      float lum = LUM_MIN + (1.0f - LUM_MIN) * profondita;
      float fr = (float)COL_ESP[0] + ((float)COL_INSP[0] - (float)COL_ESP[0]) * flusso;
      float fg = (float)COL_ESP[1] + ((float)COL_INSP[1] - (float)COL_ESP[1]) * flusso;
      float fb = (float)COL_ESP[2] + ((float)COL_INSP[2] - (float)COL_ESP[2]) * flusso;
      r = (uint8_t)(fr * lum);
      g = (uint8_t)(fg * lum);
      b = (uint8_t)(fb * lum);

      /* motori: ampiezza dalla profondita', ripartizione da 'flusso',
         inviluppo di risonanza a fRisonanza Hz che modula il tutto             */
      uint8_t env = inviluppo(fase);
      mAlto  = livelloMotore(profondita * flusso,          env);
      mBasso = livelloMotore(profondita * (1.0f - flusso), env);
      break;
    }

    /* ---- SICUREZZA: nessun battito da oltre 3 s --------------------------- */
    case ST_NO_PULSE: {
      mAlto = 0; mBasso = 0;                  /* motori fermi, senza eccezioni  */
      if (((nowMs / T_BLINK_MS) & 1UL) == 0UL) {
        r = COL_ALERT[0]; g = COL_ALERT[1]; b = COL_ALERT[2];  /* giallo ON     */
      }                                                        /* altrimenti OFF*/
      break;
    }

    /* ---- guasto fascia respiro -------------------------------------------- */
    case ST_FSR_FAULT: {
      mAlto = 0; mBasso = 0;
      if (((nowMs / (uint32_t)(T_BLINK_MS * 2)) & 1UL) == 0UL) {
        r = COL_FAULT[0]; g = COL_FAULT[1]; b = COL_FAULT[2];  /* magenta lento */
      }
      break;
    }
  }

  scriviMotori(mAlto, mBasso);                /* PWM ai MOSFET dei motori       */
  scriviLed(r, g, b);                         /* PWM ai MOSFET dei LED          */

  /* LED 13: flash breve ad ogni battito rilevato, per verificare a occhio che
     il filtro anti-vibrazione stia lavorando bene.                             */
  digitalWrite(PIN_STATUS, ((uint32_t)(nowMs - tFlashLed) < T_FLASH_MS) ? HIGH : LOW);
}


/* ===========================================================================
   18.  TELEMETRIA CSV
   =========================================================================== */

#if TELEMETRIA
void taskTelemetria(uint32_t nowMs) {
  if ((uint32_t)(nowMs - tTelemPrec) < T_TELEM_MS) return;
  tTelemPrec = nowMs;

  Serial.print(stato);            Serial.print(',');
  Serial.print((int)emaVeloce);   Serial.print(',');
  Serial.print(profondita, 3);    Serial.print(',');
  Serial.print(flusso, 3);        Serial.print(',');
  Serial.print(bpm, 1);           Serial.print(',');
  Serial.print(moltiplic);        Serial.print(',');
  Serial.println(fRisonanza, 2);
}
#endif


/* ===========================================================================
   19.  TABELLA SENO (solo se MODO_SINUSOIDALE = 1)
   =========================================================================== */

#if MODO_SINUSOIDALE
void costruisciTabellaSeno() {
  for (uint16_t i = 0; i < 256; i++) {
    float a = (float)i * (2.0f * 3.14159265f / 256.0f);
    tabSeno[i] = (uint8_t)((sin(a) * 0.5f + 0.5f) * 255.0f + 0.5f);
  }
}
#endif


/* ===========================================================================
   20.  SETUP
   =========================================================================== */

void setup() {
#if TELEMETRIA
  Serial.begin(115200);
  Serial.println(F("# NEURAL RESONANCE METHOD v1.0"));
  Serial.println(F("stato,fsr,profondita,flusso,bpm,moltiplicatore,fRisonanzaHz"));
#endif

  /* tutti i gate MOSFET come uscite, gia' a livello basso */
  pinMode(PIN_MOT_ALTO,  OUTPUT);
  pinMode(PIN_MOT_BASSO, OUTPUT);
  pinMode(PIN_LED_R,     OUTPUT);
  pinMode(PIN_LED_G,     OUTPUT);
  pinMode(PIN_LED_B,     OUTPUT);
  pinMode(PIN_STATUS,    OUTPUT);

  scriviMotori(0, 0);                     /* nessuna vibrazione all'accensione  */
  scriviLed(0, 0, 0);                     /* luci spente                        */

  configuraPWM();                         /* frequenze PWM (Timer0 intatto)     */

#if MODO_SINUSOIDALE
  costruisciTabellaSeno();
#endif

  analogRead(PIN_FSR);                    /* prima lettura scartata: il S/H     */
  analogRead(PIN_PPG);                    /* dell'ADC deve assestarsi           */

  tFsrOk    = millis();                   /* inizializzo i riferimenti tempo    */
  tShrink   = millis();
  tFasePrec = micros();

  bpm = 0.0f;
  aggiornaRisonanza();                    /* default sicuro: 40.00 Hz esatti    */
}


/* ===========================================================================
   21.  LOOP  -  cooperativo, nessuna delay() bloccante
   =========================================================================== */

void loop() {
  uint32_t nowUs = micros();              /* base tempi ad alta risoluzione     */
  uint32_t nowMs = millis();              /* base tempi in millisecondi         */

  taskPPG(nowUs, nowMs);                  /* 500 Hz - rilevamento battito       */
  taskRespiro(nowMs);                     /*  50 Hz - respiro e calibrazione    */
  taskStato(nowMs);                       /*  ogni giro - watchdog e stati      */
  taskAttuatori(nowUs, nowMs);            /*  ogni giro - fase 40 Hz + PWM      */

#if TELEMETRIA
  taskTelemetria(nowMs);                  /*  10 Hz - CSV su seriale            */
#endif
}


/* =============================================================================
   NOTE DI TARATURA AL BANCO

   1) MOT_MIN: con la fascia indossata, alzare finche' il motore parte sempre,
      poi togliere 5. Con GATE_SOGLIA = 128 il motore riceve corrente solo il
      50% del tempo, quindi MOT_MIN va tipicamente 30-50 punti piu' alto che
      in funzionamento continuo. Se gli ERM non partono proprio, portare
      GATE_SOGLIA a 180 (70% di duty ciclo).

   2) PPG_AMP_MIN: aprire il Serial Plotter, guardare l'ampiezza picco-valle
      reale sul lobo. Metterlo a circa un terzo di quel valore.

   3) COL_ALERT: le strisce RGB hanno il die verde molto piu' luminoso del
      rosso. Se il giallo di allarme vira al verde, scendere con il canale G.

   4) Se il battito viene rilevato a raffica quando i motori girano, in ordine:
      alzare T_REFRATTARIO_MS, aggiungere l'RC 1k + 1uF su A1, allontanare i
      cavi del sensore da quelli dei motori, verificare il nodo di massa unico.

   5) MODO_SINUSOIDALE = 1 richiede 256 byte di RAM in piu' per la tabella.
      Con TELEMETRIA = 1 restano comunque oltre 900 byte liberi sull'Uno.
============================================================================= */
