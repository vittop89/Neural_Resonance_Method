# Distinta base

Inserzioni verificate su Amazon.it a **settembre 2026**. I link puntano a prodotti specifici perché le compatibilità sono state controllate uno per uno (vedi [VERIFICATION.md](VERIFICATION.md)); i prezzi e la disponibilità cambiano.

Configurazione unica: non ci sono righe opzionali nella lista base.

## Lista base — 365,90 €

| # | Ruolo | Prodotto | Qtà | € |
|---|---|---|---|---|
| 1 | Microcontrollore | [Arduino UNO Rev3 (A000066)](https://www.amazon.it/dp/B008GRTSV6) | 1 | 29,30 |
| 2 | Tatto | [Dayton Audio BST-2, trasduttore tattile 35 W](https://www.amazon.it/dp/B08NN5V62G) | **2** | 103,34 |
| 3 | Pilotaggio tatto | [TPA3116 stereo 2×50 W](https://www.amazon.it/dp/B07DJ5RRS3) | 1 | 14,99 |
| 4 | Suono | [Modulo VS1053 sintetizzatore MIDI](https://www.amazon.it/dp/B07DJZ8HCL) | 1 | 12,99 |
| 5 | Luce | [Striscia RGB 12 V 5050, bobina nuda 5 m](https://www.amazon.it/dp/B07YZGMZQD) | 1 | 12,99 |
| 6 | Driver luce | [Moduli MOSFET D4184, 5 pz](https://www.amazon.it/dp/B07HBVTWMY) | 1 | 6,99 |
| 7 | Alimentazione | [Alimentatore 12 V 8 A, spina 5,5 × 2,1 mm](https://www.amazon.it/dp/B07JMNFDQM) | 1 | 19,99 |
| 8 | Da 12 V a 5 V | [Step-down LM2596, 3 A](https://www.amazon.it/dp/B0D9HSB82X) | 1 | 7,79 |
| 9 | Sensore respiro | [FSR402, 2 pz](https://www.amazon.it/dp/B0GJ53QK91) | 1 | 8,89 |
| 10 | Sensore battito | [Pulse Sensor analogico, 2 pz](https://www.amazon.it/dp/B07RD2LLK6) | 1 | 12,90 |
| 11 | Fascia toracica | [Fascia elastica regolabile 70–105 cm](https://www.amazon.it/dp/B0CNP1V41P) | 1 | 14,99 |
| 12 | Passivi | [Kit componenti DIYoung 1390 pz](https://www.amazon.it/dp/B0DXC7MX53) | 1 | 18,98 |
| 13 | Prototipazione | [Breadboard 830 + 400 punti con jumper](https://www.amazon.it/dp/B0B5TCKTQH) | 1 | 14,99 |
| 14 | Connessione 12 V | [Jack DC 5,5 × 2,1 con morsettiera, 10 coppie](https://www.amazon.it/dp/B0GSFS88MX) | 1 | 6,69 |
| 15 | Connessione striscia | [Connettori 4 pin 10 mm senza saldatura, 20 pz](https://www.amazon.it/dp/B0DMCJ7B15) | 1 | 11,99 |
| 16 | Contenitore | [Scatola ABS 250 × 150 × 100 mm](https://www.amazon.it/dp/B085NPC2R1) | 1 | 32,12 |
| 17 | Fissaggio moduli | [Distanziali e viti M3 in nylon, 580 pz](https://www.amazon.it/dp/B0H4LTLVCW) | 1 | 10,99 |
| 18 | Passivi fissi | [Basette millefori 9 × 15 cm, 10 pz](https://www.amazon.it/dp/B0BZRW8KRP) | 1 | 11,99 |
| 19 | Connettori a pannello | [GX12 aviazione 4 pin, 5 paia](https://www.amazon.it/dp/B0CRB6VQTD) | 1 | 12,99 |

**Fuori Amazon, da ferramenta:** un pannello di compensato o MDF da **15–18 mm**, circa **40 × 90 cm**, su cui bullonare i trasduttori. Pochi euro se fatto tagliare su misura, ed è la scelta di montaggio più importante del progetto — sotto i 12 mm il pannello flette e assorbe l'energia invece di trasmetterla.

## Variante "certezza" — 443,94 €

Due sostituzioni che tolgono le incognite residue.

| Prodotto | Rischio che elimina | Δ |
|---|---|---|
| [Adafruit VS1053 CODEC + MicroSD (1381)](https://www.amazon.it/dp/B0HGBTY9KQ) — 67,05 € al posto del generico da 12,99 | GPIO tutti esposti e level shifter a bordo: il modo MIDI è documentato ufficialmente e i due partitori 5 V → 3,3 V non servono più | +54,06 € |
| [Kit saldatore 80 W regolabile 13 in 1](https://www.amazon.it/dp/B09B3GRVTM) | I passivi vanno saldati su millefori; serve controllo di temperatura e punte in buono stato | +23,98 € |

## Opzionali, ordinabili dopo

| Prodotto | Quando serve | € |
|---|---|---|
| [MAX30102, 2 pz](https://www.amazon.it/dp/B07XFBZDL7) | se entrambi i Pulse Sensor deludono — sono cloni a 3,3★. PPG digitale su I²C, più immune al rumore. Va riscritto solo il blocco di lettura battito | 9,49 |
| [Oscilloscopio FNIRSI DSO-510, 10 MHz](https://www.amazon.it/dp/B0DSPT6DGR) | se ci si blocca su qualcosa che il multimetro non spiega. Le misure critiche sono coperte dalla prova in continua descritta in [WIRING.md](WIRING.md) | 59,99 |

## Strumenti

Un **multimetro con portata 2 V continui e risoluzione al millivolt** è indispensabile: senza non si regola l'LM2596 a 5,00 V, ed è il modo più comune di bruciare l'Arduino. Anche un modello economico tipo A830L basta.

Non serve un multimetro True RMS: la verifica dell'attenuatore audio si fa in continua (vedi [WIRING.md](WIRING.md)).

## Riepilogo

| Configurazione | Totale |
|---|---|
| Base | 365,90 € |
| **Consigliata** (Adafruit VS1053 + kit saldatore) | **443,94 €** |
| Con riserva PPG | 453,43 € |
| Tutto, oscilloscopio compreso | 513,42 € |

## Disposizione ibrida — opzionale, +23,98 €

Separa la portante a 40 Hz (trasduttori sotto il pannello) dallo scivolamento
alto/basso col respiro (micromotori leggeri sulla fascia toracica). Vedi
[WIRING.md](WIRING.md#disposizione-c--ibrida-il-cosa-da-sotto-il-dove-da-sopra).

| Prodotto | Qtà | € |
|---|---|---|
| [Micromotori a vibrazione 10 × 2,7 mm, 3 V, 10 pz](https://www.amazon.it/dp/B0F42P63PW) | 1 | 15,99 |
| [Modulo PCA9685, 16 canali PWM su I²C](https://www.amazon.it/dp/B0BKZC1XWR) | 1 | 7,99 |

Il PCA9685 serve perché questa disposizione richiede sette canali PWM e l'Uno
ne ha sei. In alternativa si rinuncia al canale verde della striscia LED, a
costo zero ma perdendo il giallo dello stato di allarme.

I due diodi `1N5819` e due moduli MOSFET `D4184` servono anche qui: sono già
nella lista base.
