# Neural Resonance Method

**Un apparecchio didattico di biofeedback respiratorio su Arduino Uno.** Legge il respiro e il battito cardiaco, e con quei due segnali pilota tre canali sensoriali sincronizzati: vibrazione, luce e suono.

Nato come progetto di elettronica e trattamento del segnale, documentato per essere ricostruito, misurato e — soprattutto — discusso in aula. Tutti i dimensionamenti sono verificati contro le schede tecniche dei costruttori e riportati per esteso, compresi gli errori commessi durante la progettazione e come sono stati trovati.

> **Non è un dispositivo medico.** Non va usato per diagnosi, monitoraggio clinico o terapia. Leggi [SAFETY.md](SAFETY.md) prima di costruirlo: contiene una controindicazione assoluta per chi porta dispositivi cardiaci impiantati.

---

## Cosa fa, concretamente

Due sensori in ingresso — una fascia toracica con sensore di forza, un sensore ottico di battito sul lobo dell'orecchio — da cui il firmware ricava quattro grandezze. Ciascuna pilota qualcosa di diverso:

| Grandezza | Tatto | Luce | Suono |
|---|---|---|---|
| **Profondità** del respiro (0–1) | ampiezza della vibrazione | luminosità | volume del drone ↑, risacca ↓ |
| **Direzione** insp./esp. (0–1) | ripartizione sterno ↔ addome | colore ambra ↔ blu | brillantezza del filtro |
| **Frequenza cardiaca** (BPM) | portante 30–50 Hz, fase agganciata al battito | — | — |
| **Coerenza cardiorespiratoria** (0–1) | — | saturazione del colore | intensità della campana |
| **Cambio di direzione** (evento) | — | — | campana: acuta al vertice, grave al fondo |

L'ultima riga della colonna "Luce" è la parte interessante. La saturazione del colore non segue il respiro: segue **quanto il cuore sta rispondendo al respiro**. È l'unica misura che usa entrambi i sensori insieme, e l'unica che non si può falsificare con la volontà.

---

## Perché può interessare a un insegnante di matematica e fisica

Il progetto è stato scritto con i conti in chiaro. Non è "collega questo qui e funziona": ogni valore è ricavato, e diversi valori sbagliati della prima stesura sono documentati insieme al calcolo che li ha smascherati. Vedi [docs/didattica.md](docs/didattica.md) per i percorsi completi; in sintesi:

**Matematica**
- Approssimazione all'intero più vicino con limite d'errore dimostrabile: il moltiplicatore armonico garantisce un errore < f<sub>cuore</sub>/2, cioè sotto mezzo hertz. Si dimostra in cinque righe.
- Media mobile esponenziale come filtro passa-basso discreto, e l'incrocio di due EMA come stima robusta della derivata.
- Aritmetica modulare: l'accumulatore di fase a 32 bit è un contatore mod 2³², e il traboccamento *è* il comportamento corretto.
- Legge di potenza: la correzione gamma esiste perché la percezione è logaritmica.

**Fisica**
- Filtri RC: frequenza di taglio, attenuazione in dB, resistenza equivalente di Thévenin. Il filtro del progetto attenua la portante di 45 dB e lascia passare il segnale utile intatto — e si misura con un multimetro da 18 €.
- Risonanza: il trasduttore ha Fs = 30 Hz e si usa a 40 Hz, appena sopra la risonanza.
- Dissipazione in un MOSFET: P = I²R, e il confronto fra due componenti quasi identici che a 5 V di pilotaggio si comportano in modo opposto.
- Modulazione di larghezza d'impulso, valore medio, e perché il valore medio deve restare fisso mentre l'ampiezza varia.

**Trasversale**
- Aritmia sinusale respiratoria: il battito accelera in inspirazione e rallenta in espirazione. Il progetto la misura, non la assume.

---

## Come si monta: tre disposizioni

I trasduttori tattili pesano **1,18 kg l'uno** e la loro scheda tecnica dice di installarli su una superficie piana. Non è una questione di comodità: un trasduttore inerziale funziona reagendo contro la propria massa, e ha bisogno di qualcosa di rigido a cui trasferire l'energia. Sul corpo il tessuto molle assorbe quasi tutto.

Ed è una conseguenza della fisica, non del prodotto: per muovere massa a 40 Hz servono massa ed escursione. Un attuatore leggero non può erogare forza apprezzabile a quella frequenza. **Il peso è la prestazione.**

| | Dove va la vibrazione | Cosa serve costruire |
|---|---|---|
| **A · Mobile esistente** | trasduttori avvitati sotto la seduta e dietro lo schienale di una sedia di legno, o su due doghe del letto | niente |
| **B · Pannello** | compensato o MDF 15–18 mm, 40 × 90 cm, trasduttori bullonati sotto, ci si sdraia sopra | un taglio in ferramenta |
| **C · Ibrida** | i 40 Hz restano sotto la schiena; due micromotori sulla fascia toracica portano lo scivolamento sterno ↔ addome | come B, più 23,98 € di componenti |

![Postazione supina: piantana LED che illumina il soffitto, appoggi sotto nuca e ginocchia, trasduttori bullonati sotto il pannello](docs/img/postazione-supina.svg)

La **disposizione C** separa due funzioni che hanno bisogni opposti. La portante a 40 Hz vuole forza, quindi massa, quindi un appoggio rigido: va sotto la schiena, dove la massa non costa niente. Lo scivolamento alto/basso col respiro è invece una rampa lenta di intensità, e la esegue benissimo un motorino da pochi grammi. Così si sente la vibrazione muoversi sul torace **senza caricare di massa la gabbia toracica che il dispositivo sta misurando** — che sarebbe un confondimento della misura, non solo un fastidio.

Sul firmware è un solo `#define`:

```cpp
#define DISPOSIZIONE_IBRIDA  0   // 0 = base, 1 = con i motorini sul corpo
```

A 1 i tre canali LED passano su un espansore PCA9685 via I²C, liberando due pin PWM nativi per i motorini: la disposizione ibrida richiede sette canali PWM e l'Uno ne ha sei.

Dettagli, quote e vincoli in [hardware/WIRING.md](hardware/WIRING.md#montaggio-meccanico) — compreso il perché l'imbottitura spessa spegne il dispositivo, dove vanno i LED se si medita da sdraiati, e quali punti del corpo si possono imbottire senza perdere l'accoppiamento.

## Struttura del repository

```
firmware/
  nrm_v2_trasduttori/   firmware corrente: 40 Hz reali, sintesi MIDI, coerenza
                        RSA, e il #define per la disposizione ibrida
  nrm_v1_erm/           prima versione con motori a massa eccentrica (vedi sotto)
hardware/
  BOM.md                distinta con link, prezzi e quantità
  WIRING.md             collegamenti, dimensionamenti e montaggio meccanico
  VERIFICATION.md       verifica incrociata su schede tecniche + bilancio di potenza
docs/
  didattica.md          i percorsi di matematica e fisica, con i conti
SAFETY.md               leggere prima di costruire
NOTICE.md               avvertenze legali e licenze
```

### Perché ci sono due versioni del firmware

La `v1` pilota due motori a massa eccentrica, come nel progetto iniziale. **Non è deprecata per capriccio: non può funzionare come previsto**, e il perché è istruttivo. Un motore a massa eccentrica produce vibrazione ruotando, e la sua costante di tempo meccanica è 20–50 ms; un periodo da 40 Hz dura 25 ms. Il rotore non fa in tempo né ad accelerare né a fermarsi, e quello che si percepisce è un ronzio a intensità costante.

Resta nel repo perché il confronto fra le due versioni è il pezzo più utile del progetto: stesso codice, stesso obiettivo, e un limite fisico che nessuna quantità di software può aggirare.

---

## Stato del progetto

| | |
|---|---|
| Firmware | scritto e commentato, **non ancora compilato né collaudato su hardware** |
| Dimensionamenti | verificati sulle schede dei costruttori (Dayton Audio, Adafruit, TI, AOS, Interlink) |
| Distinta | inserzioni verificate a settembre 2026 |
| Hardware fisico | non ancora costruito |

Il firmware è scritto per compilare pulito con l'IDE Arduino standard, senza librerie esterne, ma **finché non gira su una scheda vera va trattato come un progetto sulla carta**. Chi lo costruisce per primo è invitato ad aprire una issue con quello che trova.

---

## Riconoscimenti e onestà sulle fonti

Il filone di ricerca sulla stimolazione gamma a 40 Hz nasce dal lavoro del MIT sui modelli murini di Alzheimer e riguarda stimolazione **luminosa e sonora**. Gli studi sull'uomo sono preliminari, e la stimolazione **tattile** a 40 Hz è studiata molto meno delle altre due. Il nome del progetto viene da lì, ma **il progetto non pretende di riprodurre quei risultati né di verificarli**.

Quello che il progetto fa in modo documentabile è biofeedback respiratorio, che ha effetti reali e ben studiati, e che si può misurare con gli strumenti descritti qui dentro.

## Licenza

Firmware: **MIT** (vedi [LICENSE](LICENSE)).
Documentazione, schemi e distinta: **CC BY-SA 4.0**.

Avvertenze legali e controindicazioni: [NOTICE.md](NOTICE.md).

Uso didattico e personale incoraggiato. Se lo porti in classe e funziona — o se non funziona — mi fa piacere saperlo.
