# Sicurezza

**Leggere per intero prima di costruire o usare l'apparecchio.**

Questo non è un dispositivo medico. Non va usato per diagnosi, monitoraggio clinico o terapia. Il valore BPM che calcola è una stima ottica soggetta ad artefatti da movimento, non una misura clinica; l'indice di coerenza è un segnale di riscontro, non un parametro diagnostico.

---

## Controindicazione assoluta

### Dispositivi cardiaci impiantati

**Chi porta un pacemaker, un defibrillatore impiantabile (ICD) o un loop recorder non deve usare questo apparecchio.**

Questi dispositivi contengono un sensore magnetico che li commuta in modalità asincrona, o sospende le terapie antitachicardiche, sopra circa **1 mT** — soglia che il magnete di un trasduttore audio supera senza sforzo. FDA e produttori raccomandano di tenere qualunque magnete **ad almeno 15 cm** dal dispositivo impiantato, e il sito di impianto tipico è la regione pettorale sinistra.

La stessa avvertenza vale per **pompe insulina e sensori glicemici continui**, che hanno raccomandazioni analoghe.

Questa controindicazione non dipende dall'intensità del campo prodotto dall'apparecchio: dipende dal fatto che quei dispositivi sono progettati per reagire a un magnete.

---

## Per tutti gli altri

### Campi magnetici: l'intensità non è il problema

| Sorgente | Campo statico |
|---|---|
| Campo magnetico terrestre | 0,05 mT |
| Cuffie sovrauricolari, sulle orecchie | 1–3 mT |
| Trasduttore tattile, a 2–3 cm | ~2–10 mT (stima) |
| Magnete da frigorifero, a contatto | ~5 mT |
| **Limite ICNIRP, popolazione generale** | **400 mT** |
| Risonanza magnetica | 1 500–3 000 mT |

I valori dell'apparecchio stanno due ordini di grandezza sotto il limite per la popolazione generale, e nella stessa fascia di oggetti che chiunque tiene addosso ogni giorno.

**Una sfumatura onesta:** la bobina in movimento genera anche una componente *alternata* a 40 Hz, e i livelli di riferimento ICNIRP a bassa frequenza sono molto più stringenti — intorno ai 200–250 µT. A contatto un trasduttore può avvicinarli. Sono indicatori cautelativi e non soglie di danno, e asciugacapelli e piani a induzione li superano regolarmente. Non è un motivo per non costruirlo; è un motivo per non tenerlo a piena potenza per ore.

**Verificabile:** quasi tutti gli smartphone hanno un magnetometro, e una qualsiasi app di misura legge in µT. Appoggia il telefono dove andrà il corpo e leggi il numero. Vale più di qualunque stima in questa tabella.

### La luce non deve mai lampeggiare a 40 Hz

Nel progetto le luci seguono il respiro, quindi frazioni di hertz. **Questa non è una limitazione da superare: è una scelta di sicurezza permanente.**

La stimolazione luminosa intermittente può scatenare crisi in persone con epilessia fotosensibile, anche non diagnosticata. La banda di rischio maggiore sta fra 15 e 25 Hz, ma 40 Hz non è esente. I 40 Hz di questo apparecchio vivono sul canale tattile, dove questo problema non esiste.

Chi modifica il firmware per far lampeggiare i LED a frequenza alta si assume questo rischio consapevolmente, e non dovrebbe farlo con altre persone.

### Rischi elettrici

- **Il lato rete dell'alimentatore è l'unica parte del progetto che può fare male sul serio.** Alimentatore commerciale sigillato, mai aperto, mai autocostruito sui 230 V.
- I 12 V a valle sono sotto la soglia di percezione attraverso pelle asciutta.
- **Regola l'LM2596 a 5,00 V prima di collegarlo all'Arduino.** Esce di fabbrica con il trimmer in posizione arbitraria e può dare 12 V in uscita.
- Non tenere USB e alimentazione esterna sui 5 V collegati insieme: il pin `5V` scavalca il regolatore di bordo e l'USB non ha protezione su quella linea.

### Rischi meccanici e termici

- **Il calore.** La bobina di un trasduttore si scalda se pilotata a lungo. Toccala dopo dieci minuti di prova prima di fissarla vicino al corpo.
- **L'ampiezza.** Il fastidio arriva molto prima di qualunque soglia di salute, e non c'è ragione di spingere: uno stimolo appena percepibile funziona meglio. Il firmware parte da `TAT_MAX 200` e la nota di taratura dice esplicitamente di *scendere*.
- **Il peso.** Ogni trasduttore Dayton BST-2 pesa 1,18 kg. Non vanno indossati: vanno bullonati a un pannello rigido su cui ci si sdraia o siede.

---

## Chi dovrebbe parlarne con un medico prima

Chi ha una qualunque condizione cardiaca, un dispositivo impiantato, epilessia o storia di crisi, emicrania con aura, o è in gravidanza, dovrebbe parlarne con il proprio medico prima di usare un apparecchio del genere — non con una guida di elettronica.

## Se lo usi in ambito didattico

Se il progetto viene mostrato o costruito in una scuola:

- Le controindicazioni sopra vanno comunicate **prima** che chiunque lo indossi, non dopo.
- Il collegamento alla rete elettrica va fatto e supervisionato da un adulto responsabile.
- Nessuno studente dovrebbe usare l'apparecchio senza che un adulto sappia della controindicazione sui dispositivi impiantati — che può riguardare uno studente senza che la classe lo sappia.
- La parte più formativa del progetto (i calcoli, i filtri, gli errori di dimensionamento trovati) **non richiede di costruirlo né di indossarlo**.
