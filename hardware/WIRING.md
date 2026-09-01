# Collegamenti

## Mappa pin

Cinque canali PWM su sei disponibili. La scelta non è arbitraria: determina quale timer resta intatto.

| Pin | Funzione | Timer | Freq. PWM | Nota |
|---|---|---|---|---|
| A0 | FSR402 — respiro | — | — | partitore con 10 kΩ |
| A1 | Pulse Sensor — filo viola | — | — | campionato a 500 Hz |
| D1 | TX → pin `Rx` del VS1053 | — | 31250 bd | MIDI. Staccare durante l'upload |
| D8 | reset VS1053 | — | — | attivo basso, rilasciato in `setup()` |
| D9 | tattile sterno → rete RC → ampli L | Timer1 | 31 372 Hz | prescaler /1 |
| D10 | tattile addome → rete RC → ampli R | Timer1 | 31 372 Hz | prescaler /1 |
| D3 | gate MOSFET · LED rosso | Timer2 | 490 Hz | default |
| D5 | gate MOSFET · LED verde | Timer0 | 976 Hz | **prescaler mai toccato** |
| D6 | gate MOSFET · LED blu | Timer0 | 976 Hz | **prescaler mai toccato** |
| D13 | LED integrato, flash a ogni battito | — | — | diagnostica |
| liberi | D2, D4, D7, D11, D12, A2–A5 | | | riservati alla via SPI di ripiego |

**Timer0 governa `millis()`, `micros()` e `delay()`.** Il suo prescaler non viene mai modificato: `analogWrite()` sui pin 5 e 6 scrive solo il duty (OCR0A/OCR0B), non la frequenza di conteggio, quindi è sicuro. Timer1 e Timer2 sono liberi e vengono riprogrammati.

Timer1 va a 31 372 Hz perché la portante PWM deve finire il più lontano possibile dai 40 Hz: così il filtro RC la cancella completamente e resta una sinusoide pulita.

---

## Alimentazione

```
ALIMENTATORE 12 V / 8 A   (spina 5,5 × 2,1 mm)
   │
   ├── +12V ──┬── striscia LED, filo "+" (anodo comune)
   │          ├── amplificatore TPA3116  V+
   │          └── modulo buck LM2596 ──[5,0 V]── Arduino pin 5V   (NON usare VIN)
   │
   └── GND ───► NODO DI MASSA A STELLA ◄───────────────────────────┐
                     ├── Arduino GND                               │
                     ├── GND dei 3 moduli MOSFET                   │
                     ├── GND amplificatore                         │
                     ├── GND modulo VS1053                         │
                     └── negativo dell'alimentatore ────────────────┘
```

**Un solo punto fisico di massa.** Masse a catena = anelli di massa: la corrente dell'amplificatore attraversa il ritorno dei sensori e il segnale PPG diventa illeggibile.

> **Prima di collegare l'LM2596 all'Arduino, regolalo a 5,00 V con il multimetro.** Esce di fabbrica con il trimmer in posizione arbitraria e può dare 12 V. È il modo più comune di uccidere una scheda.
>
> **Non tenere USB e LM2596 collegati insieme.** Il pin `5V` scavalca il regolatore di bordo e l'USB non ha protezione su quella linea. Servono due ponticelli da sfilare prima di ogni upload — quello dei 5 V e quello verso l'`Rx` del VS1053. Montali su due header vicini fra loro e vicini alla presa USB, con un'etichetta.

---

## Canale tattile — identico per sterno e addome

```
Arduino D9 (o D10)
   │
  [10 kΩ]
   │
   ├──[1 kΩ]──── GND   ← attenuatore 1:11
   ├──[1 µF]──── GND   ← filtro: taglia a 175 Hz, uccide la portante a 31 kHz
   │
  [4,7 µF] in serie    ← blocca la continua
   │
   └──── ingresso L (o R) dell'amplificatore TPA3116

TPA3116 uscita L ──── trasduttore STERNO
TPA3116 uscita R ──── trasduttore ADDOME
```

**Nessun MOSFET su questo percorso: è un segnale, non potenza.**

### Perché l'attenuatore

Un TPA3116 ha guadagno fisso intorno ai 32 dB e arriva a fondo scala con circa **0,19 V efficaci** in ingresso. Il PWM di Arduino, filtrato, ne produrrebbe **1,4**: sette volte troppo. L'amplificatore toserebbe il segnale in permanenza, e la sinusoide a 40 Hz arriverebbe al corpo come un'onda quadra distorta — proprio la cosa che il progetto cerca di evitare.

Il partitore 10 kΩ / 1 kΩ porta il livello a **0,126 V efficaci** con `TAT_MAX = 200`.

**Dimensionamento del filtro.** La resistenza equivalente vista dal condensatore è 10 kΩ in parallelo a 1 kΩ, cioè 909 Ω. Con 1 µF:

```
f_taglio = 1 / (2π · 909 · 1e-6) ≈ 175 Hz
```

A 31 372 Hz l'attenuazione è 31372/175 ≈ 179×, cioè circa **−45 dB**: della portante non resta niente. A 40 Hz la rete è invece piatta.

### Perché l'uscita riposa a metà scala

Il PWM filtrato diventa una tensione continua: duty 0 → 0 V, duty 255 → 5 V. Se la sinusoide venisse generata fra 0 e "ampiezza", il **valore medio** cambierebbe con il respiro, e quella deriva a 0,2 Hz arriverebbe all'amplificatore come componente continua variabile: bobina spostata dal centro, riscaldamento, meno escursione utile.

Generando la sinusoide **simmetrica attorno a 128**, il valore medio resta fisso a 2,5 V qualunque cosa faccia il respiro: cambia solo l'ampiezza alternata. Anche a riposo i pin restano a 128, non a zero — continua ferma significa silenzio dopo il condensatore.

### Collaudo dell'attenuatore senza oscilloscopio

La rete è resistiva, quindi il rapporto di partizione si misura a tensione ferma. Sketch di prova:

```cpp
void setup() {
  pinMode(9, OUTPUT);
  TCCR1B = (TCCR1B & 0b11111000) | 0b001;  // 31,4 kHz come nel firmware
  analogWrite(9, 228);                     // picco della sinusoide
}
void loop() {}
```

Puntale sul nodo fra 10 kΩ, 1 kΩ e 1 µF — **prima** del condensatore da 4,7 µF, che in continua isola. Portata 2 V continui.

| `analogWrite` | Cosa rappresenta | Atteso |
|---|---|---|
| 228 | picco alto della sinusoide | 0,406 V |
| 128 | riposo, la continua fissa | 0,228 V |
| 28 | picco basso della sinusoide | 0,050 V |

La differenza fra i due picchi è **0,356 V picco-picco**; l'efficace di una sinusoide con quell'escursione vale 0,356 / (2√2) = **0,126 V**. Se i tre numeri tornano entro il 5%, l'attenuatore è giusto.

L'impedenza del nodo è 909 Ω contro i 10 MΩ del multimetro: nessun errore di carico.

---

## Canale luce — tre volte, uno per colore

```
striscia "+"  ──── +12 V   (anodo comune)
striscia R / G / B ──── V- del modulo D4184 corrispondente
modulo SIG    ──── Arduino D3 / D5 / D6
modulo GND    ──── NODO DI MASSA A STELLA
```

Commutazione low-side: duty maggiore = più luce, nessuna inversione. **Nessun diodo di ricircolo: non ci sono carichi induttivi** — a differenza della versione a motori, dove erano obbligatori.

> **Non usare più di 2 metri di striscia.** Una 5050 RGB assorbe circa 14,4 W al metro a bianco pieno. Due metri fanno 29 W; i cinque metri della bobina ne farebbero 72.

---

## Sensori

```
FSR402  (fascia toracica)
   pin 1 ──── Arduino 5V
   pin 2 ──┬─ Arduino A0
           ├─ 10 kΩ ── GND      ← partitore
           └─ 1 µF  ── GND      ← taglio a ~32 Hz, il respiro sta sotto 1 Hz

PULSE SENSOR  (lobo dell'orecchio)
   rosso  ──[100 Ω]──┬── Arduino 5V   ← filtro di alimentazione: tiene il
                     └── 100 µF ─ GND    sensore fuori dal ripple dell'ampli
   nero   ─────────────── GND (nodo a stella)
   viola  ──[1 kΩ]──┬──── Arduino A1
                    └──── 1 µF ── GND ← antialias e reiezione del ronzio

VS1053  (breakout Adafruit 1381)
   VCC    ──── Arduino 5V      i pin di interfaccia hanno i level shifter
   GND    ──── nodo a stella
   Rx     ──── Arduino D1 (TX)
   RESET  ──── Arduino D8
   GPIO-0 ──── GND      ⎫ strapping del modo MIDI, prima del rilascio del reset
   GPIO-1 ──── 3,3 V    ⎭ ⚠ i GPIO NON sono 5 V safe: usare il pin 3,3 V
   LOUT / ROUT / AGND ──── jack cuffie
```

> ⚠️ **I GPIO del VS1053 non sono 5 V safe.** I level shifter coprono i pin di interfaccia, non i GPIO. `GPIO-1` va sul pin **3,3 V** dell'Arduino, mai sui 5 V. È l'unico punto del cablaggio dove sbagliare distrugge il chip.

> Sul breakout Adafruit il pin di massa audio cambia nome fra le revisioni: **GBUF sulla v1, AGND sulla v2**. Guarda la serigrafia prima di cablare il jack cuffie.

### Le codine dell'FSR402 sono delicate

È un dispositivo a due fili con codine in film da **0,46 mm**. Saldarci sopra con un saldatore caldo scioglie il film e il sensore è perso. Interlink prescrive crimpatura o morsetto: usa una **morsettiera a vite** o uno zoccolo femmina da 2,54 mm che li stringa. Se proprio devi saldare: 250 °C e due secondi, non uno di più.

### Tensione della fascia

L'FSR402 ha **fondo scala a 20 N** (~2 kg) su un'area attiva di soli Ø 14,7 mm: una fascia toracica ben tesa ci arriva facilmente, e sopra quella soglia satura. **Allenta la fascia finché la lettura a riposo sta a metà scala (400–600 conteggi).** Il partitore da 10 kΩ va bene: prima di cambiarlo, sistema la tensione della fascia.

### Cablaggio

Tieni i cavi del PPG lontani da quelli che vanno ai trasduttori, incrociali a 90° se devono passarsi accanto, e attorciglia a coppia i due fili di ciascun trasduttore. Un elettrolitico da 470–1000 µF sul +12 V vicino all'amplificatore assorbe i picchi di corrente che altrimenti fanno oscillare l'intera rete.

---

## Montaggio meccanico

### I trasduttori non si indossano

Il BST-2 pesa **1,18 kg** ciascuno, misura 131 mm di diametro per 55 mm, e ha quattro fori di fissaggio con **interasse 127 mm**. La scheda tecnica dice "facile da installare su qualsiasi superficie piana".

Il motivo non è solo il peso. Un trasduttore inerziale funziona **reagendo contro la propria massa**: ha bisogno di una superficie rigida a cui trasferire l'energia. Appoggiato sulla pelle, il tessuto molle assorbe quasi tutto. Questi oggetti sono progettati per essere avvitati ai mobili — è il loro mercato d'origine.

**È una conseguenza della fisica, non del prodotto.** Per muovere massa a 40 Hz servono massa e escursione: un trasduttore leggero non può erogare forza apprezzabile a quella frequenza. Il peso *è* la prestazione. Chi volesse qualcosa di indossabile deve rinunciare ai 40 Hz reali e tornare a un attuatore che modula solo l'intensità — cioè alla `v1`.

### Tre modi di montarli, in ordine di fatica

**1. Mobile che hai già — zero costruzione.** Quattro viti da legno su una superficie piana e rigida:
- sotto la seduta e dietro lo schienale di una **sedia di legno**
- su due **doghe del letto**, all'altezza delle spalle e del bacino
- sotto un **panchetto da meditazione** o una tavola di legno massello

Serve solo che il legno sia spesso e non flessibile.

**2. Pannello dedicato.** Compensato o MDF da **15–18 mm**, circa **40 × 90 cm**, tagliato in ferramenta. I trasduttori si bullonano *sotto*: uno all'altezza delle scapole, uno all'altezza del bacino. Ci si sdraia sopra con un tappetino sottile in mezzo.

Sotto i 12 mm il pannello flette e assorbe l'energia invece di trasmetterla.

**3. Due pannelli separati**, uno per trasduttore, circa 40 × 40 cm ciascuno. Si spostano e si ripongono meglio, ma tendono a slittare.

In tutti i casi, la distanza fra i due punti diventa **50 cm invece dei 20 fra sterno e addome**: la distinzione fra alto e basso è più netta che nella disposizione indossata.

### Comfort contro trasmissione: la stessa gommapiuma, due effetti opposti

Una sessione di meditazione dura decine di minuti, e un pannello di compensato nudo diventa scomodo in fretta. La tentazione è metterci sopra un'imbottitura spessa. **È esattamente la cosa che spegne il dispositivo.**

Corpo più imbottitura formano un sistema massa-molla con la sua frequenza propria:

$$f_0 = \frac{1}{2\pi}\sqrt{\frac{k}{m}}$$

Un materasso o una gommapiuma morbida sotto un corpo hanno $f_0$ dell'ordine di **3–8 Hz**. La trasmissibilità crolla per $f > \sqrt{2}\,f_0$, quindi a 40 Hz siamo cinque volte sopra: l'imbottitura **isola**, che è precisamente il suo mestiere. È lo stesso principio degli antivibranti sotto le lavatrici.

**La simmetria da tenere a mente:** la gommapiuma che metti *sotto* il pannello per proteggere il pavimento, messa *sopra*, ti isolerebbe dalla vibrazione che vuoi sentire. Stesso materiale, stessa fisica, intenzione opposta.

**Regola pratica: sottile e denso trasmette, spesso e morbido isola.**

| Interfaccia | Spessore | Effetto a 40 Hz |
|---|---|---|
| Compensato nudo | — | trasmette tutto, scomodo dopo dieci minuti |
| Tappetino yoga o feltro denso | 5–10 mm | trasmette bene, comfort accettabile |
| Tappetino da campeggio | 20–40 mm | attenua parecchio |
| Materasso, cuscino morbido | > 50 mm | isola: non senti quasi niente |

**Imbottisci dove non c'è il trasduttore.** Testa, nuca e ginocchia possono avere tutto il cuscino che vuoi: lì non passa segnale. Sopra le due zone dei trasduttori serve invece contatto sottile e fermo. È una soluzione a chiazze, non uniforme, e funziona.

### Da seduti, che per la meditazione è meglio

Il contatto migliore non è la schiena: sono le **tuberosità ischiatiche**, le ossa su cui ci si siede. L'osso conduce la vibrazione, il tessuto molle la assorbe, e da seduti tutto il peso passa per due punti ossei.

Uno **sgabello rigido o una panchetta da meditazione**, con un trasduttore avvitato sotto la seduta e uno dietro uno schienale rigido:

- il trasduttore *basso* lavora contro le ossa ischiatiche — l'accoppiamento migliore disponibile sul corpo umano;
- il trasduttore *alto* lavora sulla parte alta della schiena;
- la mappatura respiro → alto/basso resta identica a quella pensata per sterno e addome, e la postura è quella in cui si medita davvero;
- un cuscino sottile e denso sopra la seduta è tollerabile per un'ora.

Per sessioni lunghe questa è la disposizione da preferire. Il pannello da sdraiati resta valido per sessioni brevi o per chi medita in posizione supina.

### Disaccoppiare dal pavimento

Un pannello appoggiato direttamente su un pavimento rigido fa due cose sbagliate: il pavimento smorza la vibrazione, e il resto se ne va nella struttura dell'edificio. **In appartamento un trasduttore da 35 W a 40 Hz si sente dai vicini.**

Metti il pannello su **blocchi di gommapiuma densa**, su un tappeto spesso, o su quattro piedini antivibranti. Costa poco e migliora sia la resa sia i rapporti di condominio.

### Lunghezza dei cavi

I cavi del BST-2 sono **già attaccati e lunghi 61 cm**. Dal pannello alla scatola potrebbero non bastare: tieni pronta una prolunga a due conduttori per lato, o piazza la scatola vicino al pannello.

### L'elettronica

Sei moduli da fissare più una millefori: servono circa **180 cm²** di superficie utile, che con il margine per il cablaggio diventano 250 cm² abbondanti. Il contenitore da 250 × 150 mm dà circa 351 cm² interni, cioè il 51% di riempimento.

| Cosa | Come | Perché così |
|---|---|---|
| Arduino, VS1053, LM2596, MOSFET | distanziali M3 in nylon, 6–10 mm | Il nylon non conduce: nessun rischio di cortocircuitare una pista sul retro |
| TPA3116 | distanziali 10–15 mm, lontano dagli altri | Ha un dissipatore che scalda, e sta lontano dai fili dei sensori: è la sorgente di rumore più forte della scatola |
| Passivi (RC, filtri) | saldati su millefori 9 × 15 cm | Su breadboard un contatto ossidato produce sintomi che sembrano bug del firmware |
| Cavi verso il corpo | connettori GX12 a 4 pin sul pannello | La fascia si stacca senza aprire niente, e la tensione va sul connettore, non sulla saldatura |
| Alimentazione | presa jack DC da pannello + passacavo | La spina entra dalla scatola, non da un buco fatto col trapano |

**Harness sensori:** 4 conduttori — 5 V, GND, segnale FSR, segnale PPG. Esattamente un GX12 a 4 pin.
**Harness trasduttori:** 4 conduttori, due coppie. GX12 da 5 A contro un picco di ~2 A per canale.
