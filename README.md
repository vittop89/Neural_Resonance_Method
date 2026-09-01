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
- Filtri RC: frequenza di taglio, attenuazione in dB, resistenza equivalente di Thévenin. Il filtro del progetto attenua la portante di 46 dB e lascia passare il segnale utile intatto — e si misura con un multimetro da 18 €.
- Risonanza: il trasduttore ha Fs = 30 Hz e si usa a 40 Hz, appena sopra la risonanza.
- Dissipazione in un MOSFET: P = I²R, e il confronto fra due componenti quasi identici che a 5 V di pilotaggio si comportano in modo opposto.
- Modulazione di larghezza d'impulso, valore medio, e perché il valore medio deve restare fisso mentre l'ampiezza varia.

**Trasversale**
- Aritmia sinusale respiratoria: il battito accelera in inspirazione e rallenta in espirazione. Il progetto la misura, non la assume.

---

## Struttura del repository

```
firmware/
  nrm_v2_trasduttori/   firmware corrente: 40 Hz reali, sintesi MIDI, coerenza RSA
  nrm_v1_erm/           prima versione con motori a massa eccentrica (vedi sotto)
hardware/
  BOM.md                distinta con link, prezzi e quantità
  WIRING.md             schema dei collegamenti, nodo per nodo
  VERIFICATION.md       verifica incrociata su schede tecniche + bilancio di potenza
docs/
  didattica.md          i percorsi di matematica e fisica, con i conti
SAFETY.md               leggere prima di costruire
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
