# Verifica incrociata

Ogni componente controllato sulla documentazione del costruttore, non sull'inserzione del venditore. Questo documento riporta anche gli **errori commessi durante la progettazione**, perché il modo in cui sono stati trovati è la parte più utile del progetto.

Fonti consultate: Dayton Audio (scheda BST-2), Adafruit (guida ufficiale VS1053 breakout 1381), Texas Instruments (TPA3116D2), Alpha & Omega Semiconductor (AOD4184), Interlink Electronics (FSR 400 Series).

---

## Errori trovati e corretti

### 1. L'amplificatore sarebbe andato in saturazione — fattore 7

**Sintomo che avrebbe avuto:** vibrazione presente ma "sporca", niente affatto la sinusoide pulita a 40 Hz che il progetto promette.

Un TPA3116 ha guadagno fisso intorno ai 32 dB e arriva a fondo scala con circa 0,19 V efficaci. Il PWM di Arduino filtrato ne produce **1,4 V efficaci**: sette volte troppo. L'amplificatore avrebbe tosato il segnale in permanenza.

**Correzione:** aggiunto un attenuatore 10 kΩ / 1 kΩ prima del filtro. Livello risultante 0,126 V efficaci. Vedi [WIRING.md](WIRING.md) per il dimensionamento completo.

### 2. Deriva di continua sul trasduttore

La prima stesura generava la sinusoide fra 0 e "ampiezza", quindi il **valore medio** cambiava con il respiro. Quella deriva a 0,2 Hz sarebbe arrivata all'amplificatore come continua variabile: bobina spostata dal centro, riscaldamento, meno escursione utile.

**Correzione:** sinusoide simmetrica attorno a 128, valore medio fisso a 2,5 V. Anche a riposo i pin restano a 128, non a zero.

### 3. L'alimentatore aveva l'uscita sbagliata

Il modello inizialmente scelto (12 V 10 A) è un adattatore con **presa accendisigari da auto**, non un jack. Non si collega a niente in questo progetto.

**Correzione:** sostituito con un 12 V 8 A che dichiara esplicitamente la spina 5,5 × 2,1 mm, la stessa misura dei jack in distinta. Costa anche meno, e gli 8 A restano abbondanti.

### 4. Due trasduttori pesano 2,36 kg

La scheda Dayton dà il BST-2 a **2,6 lb = 1,18 kg ciascuno**, con fori di fissaggio a interasse **127 mm**. L'ipotesi di indossarli sul torace, lasciata aperta nelle prime stesure, va scartata del tutto: non è comodità, è fattibilità. Le piastrine da 10 × 10 cm suggerite in precedenza erano più piccole dei fori stessi.

**Correzione:** unica disposizione è il pannello rigido su cui sdraiarsi.

---

## Il dato che valida il progetto

Era l'assunzione più rischiosa di tutte — che un trasduttore commerciale rendesse bene a 40 Hz — ed è confermata dal costruttore con margine.

| Parametro BST-2 | Valore | Conseguenza |
|---|---|---|
| Risposta in frequenza | **10–80 Hz** | 40 Hz cade al centro esatto della banda |
| Frequenza di risonanza Fs | 30 Hz | 40 Hz è appena sopra la risonanza: zona di massima resa |
| Impedenza | 4 Ω | il TPA3116 dichiara 4/6/8 Ω |
| Potenza | 35 W RMS | l'ampli a 12 V ne eroga ~15: si lavora al 43%, con margine termico |
| Forza di picco | 25 lb/ft | ampiamente sufficiente attraverso un pannello |
| Filtro consigliato | passa-basso < 80 Hz | la rete RC del progetto taglia già a 175 Hz |

---

## Il canale MIDI è documentato ufficialmente

Per il breakout Adafruit 1381 la documentazione ufficiale chiude tre questioni:

- **Tutti gli 8 GPIO sono esposti.** I ponticelli del modo MIDI si fanno: `GPIO-0 → GND`, `GPIO-1 → 3,3 V`.
- **C'è un pin `Rx` dedicato** che accetta MIDI a 31250 baud. Nessun SPI, nessun plugin da caricare.
- **I pin di interfaccia hanno i level shifter a bordo e sono 5 V compatibili.** Alimentazione `VCC → 5 V`.

Esiste anche uno sketch di riferimento: `File → Esempi → Adafruit_VS1053_Codec → player_miditest`. Se quello suona, il canale audio funziona.

**Conseguenza:** con il breakout Adafruit i partitori 5 V → 3,3 V sulle linee TX e RESET non servono. Erano l'assicurazione contro un modulo generico privo di traslatori.

**Eccezione, scritta in rosso nella guida:** i level shifter coprono i pin di interfaccia, **non i GPIO**. `GPIO-1` va sui 3,3 V, mai sui 5 V.

---

## Tabella delle interfacce

| Interfaccia | Lato A | Lato B | Esito |
|---|---|---|---|
| Rete → scatola | PSU spina 5,5 × 2,1 mm | jack 5,5 × 2,1 mm | ✅ combaciano |
| 12 V → 5 V | ingresso LM2596 3,2–40 V | uscita 1,25–35 V, 2 A continui | ✅ carico 0,15 A |
| Arduino → MOSFET | uscita 5 V | AOD4184, 6,5 mΩ a V<sub>GS</sub> 4,5 V | ✅ logic level |
| MOSFET → striscia | 40 V / 50 A | 0,8 A per canale su 2 m | ✅ margine 60× |
| Arduino → ampli | rete RC, 0,126 V eff. | TPA3116 con pot volume | ✅ in finestra |
| Alimentazione ampli | 12 V | TPA3116, 4,5–26 V | ✅ in banda |
| Ampli → trasduttori | 4/6/8 Ω dichiarati | BST-2, 4 Ω | ✅ compatibile |
| Segnale → trasduttori | sinusoide 30–50 Hz | BST-2, 10–80 Hz | ✅ centro banda |
| Arduino → VS1053 | logica 5 V | level shifter a bordo | ✅ diretto |
| Arduino 3,3 V → GPIO-1 | pin 3,3 V | GPIO non 5 V safe | ⚠️ solo 3,3 V |
| Harness sensori | 4 conduttori | GX12 a 4 pin | ✅ esatto |
| Harness trasduttori | 2 coppie, picco ~2 A | GX12 a 4 pin, 5 A | ✅ margine 2,5× |
| Moduli → scatola | ~180 cm² + cablaggio | ~351 cm² interni | ✅ 51% riempimento |

---

## Bilancio di potenza

| Carico | Tipico | Massimo |
|---|---|---|
| Striscia LED, 2 m | 1,5 A | 2,4 A |
| TPA3116 con 2 × BST-2 | 0,5 A | 2,5 A |
| Arduino + VS1053, via LM2596 | 0,1 A | 0,2 A |
| **Totale** | **2,1 A** | **5,1 A** |
| **Alimentatore** | | **8 A** |

Margine del 57% sul caso peggiore, e il caso peggiore presuppone striscia a bianco pieno **e** amplificatore a fondo corsa contemporaneamente — una condizione che questo apparecchio non raggiunge mai in uso.

---

## Consigli ribaltati dalla verifica

Tre affermazioni delle stesure precedenti che il controllo sulle schede tecniche ha smentito.

| Detto prima | Il dato reale | Ora |
|---|---|---|
| «Se l'escursione dell'FSR è sotto 60 conteggi, passa da 10 kΩ a 47 o 100 kΩ» | L'FSR402 ha **fondo scala a 20 N** (~2 kg) su area attiva Ø 14,7 mm. Una fascia toracica ben tesa ci arriva. | Il rischio è l'opposto: **saturazione da fascia troppo stretta**. Prima di cambiare resistenza, allenta la fascia finché il riposo sta a metà scala. |
| «L'IRF520 va sostituito perché a 5 V conduce male» | L'AOD4184 dichiara **R<sub>DS(on)</sub> = 6,5 mΩ a V<sub>GS</sub> = 4,5 V**, contro 5,0 mΩ a 10 V. | Confermato con i numeri: a 0,8 A per canale dissipa **4 mW**. Il MOSFET resta freddo. |
| «Un multimetro True RMS misura i 0,126 V dell'attenuatore» | I multimetri economici hanno la portata alternata più bassa a 200 V e non sono True RMS. | Si verifica in continua con la prova a tre punti in [WIRING.md](WIRING.md). Nessuno strumento aggiuntivo. |

---

## Cosa resta non verificato

Onestà sui limiti di questa verifica:

- **Il firmware non è mai stato compilato né caricato su una scheda.** È scritto per compilare pulito con l'IDE Arduino standard e senza librerie esterne, ma finché non gira è un progetto sulla carta.
- **Il guadagno esatto della scheda TPA3116** non è dichiarato dal venditore. Le schede economiche usano tipicamente 32 o 36 dB. Il progetto è robusto all'incertezza perché ci sono tre regolazioni indipendenti: il potenziometro della scheda, la costante `TAT_MAX` e la resistenza da 1 kΩ dell'attenuatore.
- **Nessun componente è stato misurato fisicamente.** Tutti i numeri vengono da schede tecniche.
