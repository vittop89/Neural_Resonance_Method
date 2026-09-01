# Distinta base

Inserzioni verificate su Amazon.it a **settembre 2026**. I link puntano a prodotti specifici perché le compatibilità sono state controllate una per una: vedi [VERIFICATION.md](VERIFICATION.md).

Il progetto è su **ESP32**. La versione su Arduino Uno resta nel repository come `firmware/nrm_v2_trasduttori`, ma non è più la strada consigliata — [il perché è spiegato nel README](../README.md#perché-esp32-e-non-arduino).

## Lista base — 360,18 €

| # | Ruolo | Prodotto | Qtà | € |
|---|---|---|---|---|
| 1 | Microcontrollore | [ESP32-WROOM-32D DevKit V4, 38 pin](https://www.amazon.it/dp/B0C5HD6ZGJ) | 1 | 14,99 |
| 2 | Tatto | [Dayton Audio BST-2, trasduttore tattile 35 W](https://www.amazon.it/dp/B08NN5V62G) | **2** | 103,34 |
| 3 | Pilotaggio tatto | [TPA3116 stereo 2×50 W](https://www.amazon.it/dp/B07DJ5RRS3) | 1 | 14,99 |
| 4 | Uscita audio | [Modulo DAC I²S PCM5102A](https://www.amazon.it/dp/B0GCK53SKK) | 1 | 13,99 |
| 5 | Adattamento logica | [SN74HCT245N DIP-20, 10 pz](https://www.amazon.it/dp/B0DQ5WRHS3) | 1 | 11,00 |
| 6 | Luce | [Striscia RGB 12 V 5050, bobina nuda 5 m](https://www.amazon.it/dp/B07YZGMZQD) | 1 | 12,99 |
| 7 | Driver luce | [Moduli MOSFET D4184, 5 pz](https://www.amazon.it/dp/B07HBVTWMY) | 1 | 6,99 |
| 8 | Alimentazione | [Alimentatore 12 V 8 A, spina 5,5 × 2,1 mm](https://www.amazon.it/dp/B07JMNFDQM) | 1 | 19,99 |
| 9 | Da 12 V a 5 V | [Step-down LM2596, 3 A](https://www.amazon.it/dp/B0D9HSB82X) | 1 | 7,79 |
| 10 | Sensore respiro | [FSR402, 2 pz](https://www.amazon.it/dp/B0GJ53QK91) | 1 | 8,89 |
| 11 | Sensore battito | [MAX30102, 2 pz](https://www.amazon.it/dp/B07XFBZDL7) | 1 | 9,49 |
| 12 | Fascia toracica | [Fascia elastica regolabile 70–105 cm](https://www.amazon.it/dp/B0CNP1V41P) | 1 | 14,99 |
| 13 | Passivi | [Kit componenti DIYoung 1390 pz](https://www.amazon.it/dp/B0DXC7MX53) | 1 | 18,98 |
| 14 | Prototipazione | [Breadboard 830 + 400 punti con jumper](https://www.amazon.it/dp/B0B5TCKTQH) | 1 | 14,99 |
| 15 | Connessione 12 V | [Jack DC 5,5 × 2,1 con morsettiera, 10 coppie](https://www.amazon.it/dp/B0GSFS88MX) | 1 | 6,69 |
| 16 | Connessione striscia | [Connettori 4 pin 10 mm senza saldatura, 20 pz](https://www.amazon.it/dp/B0DMCJ7B15) | 1 | 11,99 |
| 17 | Contenitore | [Scatola ABS 250 × 150 × 100 mm](https://www.amazon.it/dp/B085NPC2R1) | 1 | 32,12 |
| 18 | Fissaggio moduli | [Distanziali e viti M3 in nylon, 580 pz](https://www.amazon.it/dp/B0H4LTLVCW) | 1 | 10,99 |
| 19 | Passivi fissi | [Basette millefori 9 × 15 cm, 10 pz](https://www.amazon.it/dp/B0BZRW8KRP) | 1 | 11,99 |
| 20 | Connettori a pannello | [GX12 aviazione 4 pin, 5 paia](https://www.amazon.it/dp/B0CRB6VQTD) | 1 | 12,99 |

**Fuori Amazon, da ferramenta:** un pannello di compensato o MDF da **15–18 mm**, circa **40 × 90 cm**, su cui bullonare i trasduttori. Pochi euro se fatto tagliare su misura, ed è la scelta di montaggio più importante del progetto.

## Cosa è cambiato passando all'ESP32

Tre componenti sono spariti dalla distinta e uno è stato sostituito con qualcosa di migliore.

| | Su Arduino Uno | Su ESP32 | Δ |
|---|---|---|---|
| Microcontrollore | Uno Rev3, 29,30 € | ESP32 DevKit, 14,99 € | −14,31 |
| Sintesi audio | VS1053, 12,99 € (o **67,05 €** nella variante certezza) | DAC I²S PCM5102A, 13,99 € — **il suono è generato in software** | +1,00, ma **−54,06** sulla variante certezza |
| Espansione PWM | PCA9685, 7,99 € nella disposizione ibrida | non serve: 16 canali LEDC nativi | −7,99 |
| Sensore battito | Pulse Sensor analogico 3,3★, 12,90 € | MAX30102 digitale, 9,49 € | −3,41 |
| Adattamento logica | non serviva (5 V nativi) | SN74HCT245, 11,00 € | +11,00 |

Il MAX30102 diventa la scelta ovvia e non più un ripiego: è un dispositivo **a 3,3 V**, cioè esattamente la logica dell'ESP32. Sull'Uno avrebbe richiesto un traslatore; qui si collega diretto, e porta con sé un convertitore dedicato e la reiezione della luce ambientale.

## Variante consigliata — 384,16 €

Una sola aggiunta, e non è un componente.

| Prodotto | Perché | € |
|---|---|---|
| [Kit saldatore 80 W regolabile 13 in 1](https://www.amazon.it/dp/B09B3GRVTM) | I passivi e il 74HCT245 vanno saldati su millefori; serve controllo di temperatura e punte in buono stato | 23,98 |

**Non esiste più una "variante certezza".** Sull'Uno serviva perché il modo MIDI del VS1053 era l'unico punto del progetto che poteva non funzionare al primo colpo, e la scheda Adafruit che lo garantiva costava 54 € in più. Qui il VS1053 non c'è: il suono lo genera il microcontrollore.

## Disposizione ibrida — opzionale, +15,99 €

Separa la portante a 40 Hz (trasduttori sotto il pannello) dallo scivolamento alto/basso col respiro (micromotori sulla fascia toracica). Vedi [WIRING.md](WIRING.md#disposizione-c--ibrida-il-cosa-da-sotto-il-dove-da-sopra).

| Prodotto | Qtà | € |
|---|---|---|
| [Micromotori a vibrazione 10 × 2,7 mm, 3 V, 10 pz](https://www.amazon.it/dp/B0F42P63PW) | 1 | 15,99 |

Sull'Uno serviva anche un PCA9685, perché la disposizione ibrida chiedeva sette canali PWM su sei disponibili. **Sull'ESP32 il problema non esiste**: il LEDC ha 16 canali indipendenti, e ne restano undici liberi. I due diodi `1N5819` per i motorini e due moduli `D4184` sono già nella lista base.

## Opzionale

| Prodotto | Quando serve | € |
|---|---|---|
| [Oscilloscopio FNIRSI DSO-510, 10 MHz](https://www.amazon.it/dp/B0DSPT6DGR) | Meno utile che sull'Uno: senza portante PWM da verificare, le misure critiche si riducono al livello d'uscita del DAC. Serve solo se ci si blocca su qualcosa che il multimetro non spiega | 59,99 |

## Strumenti

Un **multimetro con portata 2 V continui e risoluzione al millivolt** resta indispensabile: senza non si regola l'LM2596 a 5,00 V prima di collegarlo, ed è il modo più comune di bruciare una scheda. Un modello economico tipo A830L basta.

## Riepilogo

| Configurazione | Totale |
|---|---|
| Base | 360,18 € |
| **Consigliata** (con kit saldatore) | **384,16 €** |
| Con disposizione ibrida | 400,15 € |
| Tutto, oscilloscopio compreso | 460,14 € |

Per confronto, la stessa configurazione su Arduino Uno costava **443,94 €**: l'ESP32 fa risparmiare quasi sessanta euro *e* elimina il componente più incerto del progetto.
