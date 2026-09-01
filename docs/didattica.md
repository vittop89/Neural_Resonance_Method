# Percorsi didattici

Il progetto è stato scritto con i conti in chiaro, perché la parte istruttiva non è l'apparecchio finito: sono i dimensionamenti, e gli errori che li hanno preceduti.

**Quasi tutto quello che segue si può fare senza costruire niente.** I calcoli, i filtri, il limite d'errore dell'approssimazione e il confronto fra i due MOSFET stanno in piedi da soli, con carta e calcolatrice. Costruire l'apparecchio serve a verificarli, non a capirli.

Livello: secondo biennio e quinto anno di liceo scientifico o istituto tecnico.

---

## 1. Approssimazione all'intero più vicino, con limite d'errore dimostrabile

**Matematica — analisi elementare, stima dell'errore**

Il dispositivo deve far vibrare i trasduttori a una frequenza che sia insieme un **multiplo intero** della frequenza cardiaca e il più vicina possibile a 40 Hz.

Sia $f_c$ la frequenza cardiaca in hertz (BPM / 60) e $x = 40/f_c$. Si sceglie

$$M = \mathrm{round}(x), \qquad f_{ris} = M \cdot f_c$$

**Domanda per la classe: quanto può sbagliare, al massimo?**

Per definizione di arrotondamento, $|M - x| \le \tfrac{1}{2}$. Quindi

$$|f_{ris} - 40| = |M f_c - x f_c| = f_c \, |M - x| \le \frac{f_c}{2}$$

L'errore massimo è **metà della frequenza cardiaca**, e non dipende dal valore obiettivo. A 60 BPM ($f_c = 1$ Hz) l'errore non supera 0,5 Hz; a 48 BPM non supera 0,4 Hz.

Tre righe, e il risultato è forte: più il cuore è lento — cioè più la meditazione funziona — più la risonanza è precisa.

**Estensioni**
- Errore *relativo*: $|f_{ris}-40|/40 \le f_c/80$. A 60 BPM è sotto l'1,3%.
- Per quali frequenze cardiache l'errore è **esattamente zero**? Serve $40/f_c \in \mathbb{N}$, cioè $f_c = 40/k$, cioè BPM $= 2400/k$. Per $k = 40, 50, 32$ si ottengono 60, 48 e 75 BPM. Bell'esercizio sui divisori di 2400.
- Che succede se il target non fosse 40 ma un numero irrazionale? Il limite d'errore regge lo stesso: dipende solo dall'arrotondamento.

Codice corrispondente: funzione `aggiornaRisonanza()` in [`nrm_v2_trasduttori.ino`](../firmware/nrm_v2_trasduttori/nrm_v2_trasduttori.ino).

---

## 2. La media mobile esponenziale *è* un filtro RC

**Matematica e fisica insieme — è il collegamento più bello del progetto**

Il firmware filtra il segnale del respiro così:

$$y_n = y_{n-1} + \alpha\,(x_n - y_{n-1}) = (1-\alpha)\,y_{n-1} + \alpha\,x_n$$

**Risposta al gradino.** Con $x_n = 1$ e $y_0 = 0$ si ricava per induzione $y_n = 1 - (1-\alpha)^n$. Il tempo per raggiungere il 63% ($1 - 1/e$) è

$$n_\tau = \frac{-1}{\ln(1-\alpha)}$$

Con $\alpha = 0{,}25$ e campionamento a 50 Hz (periodo 20 ms): $n_\tau = 3{,}48$ campioni, cioè $\tau \approx 70$ ms.
Con $\alpha = 0{,}06$: $n_\tau = 16{,}2$ campioni, cioè $\tau \approx 320$ ms.

**Il punto.** Un circuito RC ha risposta al gradino $1 - e^{-t/RC}$, con $\tau = RC$. La formula discreta e quella continua descrivono lo stesso comportamento: l'EMA **è** un passa-basso RC campionato, e vale la corrispondenza

$$\alpha = \frac{T}{T + RC}$$

Frequenza di taglio equivalente: $f_t = 1/(2\pi\tau)$, cioè 2,3 Hz per l'EMA veloce e 0,49 Hz per quella lenta.

Si può far ricavare la corrispondenza alla classe, poi verificarla: stesso segnale, un ramo filtrato in software e uno con un RC vero, e i due grafici si sovrappongono.

**Perché nel progetto ce ne sono due.** La differenza fra EMA veloce e EMA lenta è una stima robusta della *derivata* del segnale: è positiva quando il respiro sale, negativa quando scende. Serve a decidere se si sta inspirando o espirando senza derivare numericamente un segnale rumoroso — che amplificherebbe il rumore invece del segnale.

---

## 3. Dimensionare un filtro RC per davvero

**Fisica — reattanza, decibel, teorema di Thévenin**

La rete che porta il segnale all'amplificatore fa tre cose insieme: attenua, filtra e blocca la continua.

```
D9 ──[10 kΩ]──┬──[1 kΩ]── GND
              ├──[1 µF]── GND
              └──[4,7 µF]── ingresso amplificatore
```

**Attenuazione (partitore resistivo)**

$$A = \frac{R_2}{R_1+R_2} = \frac{1}{11} = 0{,}0909 \quad \Rightarrow \quad 20\log_{10}(0{,}0909) = -20{,}8 \text{ dB}$$

**Frequenza di taglio.** Il condensatore non vede $R_1$ né $R_2$ separatamente, ma la resistenza equivalente di Thévenin:

$$R_{th} = R_1 \parallel R_2 = \frac{10000 \cdot 1000}{11000} = 909\ \Omega$$

$$f_t = \frac{1}{2\pi R_{th} C} = \frac{1}{2\pi \cdot 909 \cdot 10^{-6}} \approx 175\ \text{Hz}$$

**Verifica alle due frequenze che contano.** Con $|H(f)| = 1/\sqrt{1+(f/f_t)^2}$:

| Frequenza | Cos'è | $|H|$ | dB |
|---|---|---|---|
| 40 Hz | il segnale utile | 0,975 | **−0,2 dB** (praticamente intatto) |
| 31 372 Hz | la portante PWM da eliminare | 0,0056 | **−45 dB** (sparita) |

Il progetto funziona proprio grazie a questo rapporto: la portante sta 784 volte sopra il segnale, quindi un filtro del prim'ordine basta e avanza.

**Esercizio.** Se si volesse una portante a 3,9 kHz invece di 31 kHz (prescaler /8 invece di /1), quale sarebbe l'attenuazione? Basterebbe ancora? *Risposta: −27 dB invece di −45. Passerebbe circa 8 volte più residuo di portante.*

---

## 4. Aritmetica modulare che gira su un contatore a 32 bit

**Matematica discreta — e un caso in cui il traboccamento è il comportamento voluto**

La fase dell'oscillatore è una variabile intera senza segno a 32 bit. Un giro completo corrisponde a $2^{32}$. L'incremento per microsecondo vale

$$\Delta = f \cdot \frac{2^{32}}{10^6} = f \cdot 4294{,}967296$$

A 40 Hz: $\Delta = 171\,798{,}69$, troncato a $171\,798$.

**Errore introdotto dal troncamento:**

$$\frac{171798}{171798{,}69} = 0{,}99999597 \quad \Rightarrow \quad f_{reale} = 39{,}99984\ \text{Hz}$$

Un errore di $1{,}6 \times 10^{-4}$ Hz: quattro ordini di grandezza sotto la precisione che serve.

**Il punto interessante.** Quando l'accumulatore supera $2^{32}$ *trabocca*, e il traboccamento non è un bug: è esattamente la riduzione modulo $2^{32}$, cioè il ritorno a zero della fase dopo un giro completo. L'hardware fa gratis l'aritmetica modulare che servirebbe scrivere a mano.

Buon punto di partenza per parlare di rappresentazione dei numeri, di $\mathbb{Z}/n\mathbb{Z}$ e del perché il software di solito teme il traboccamento mentre qui lo sfrutta.

---

## 5. La percezione non è lineare

**Fisica e psicofisica — leggi di potenza**

Una rampa lineare di PWM su un LED non si vede come una rampa di luminosità: sembra che non succeda niente per metà corsa e poi tutto insieme. La risposta dell'occhio segue approssimativamente una legge di potenza (Stevens) con esponente vicino a $1/2{,}2$:

$$\text{percepito} \propto L^{1/2{,}2}$$

Per ottenere una dissolvenza percepita come lineare bisogna precompensare con l'esponente inverso. Il firmware usa l'esponente 2, che si calcola con soli interi:

```cpp
uint8_t gamma2(uint8_t v) { return ((uint16_t)v * v) / 255; }
```

**Esperimento.** Due strisce LED affiancate, una pilotata linearmente e una con la correzione, che salgono nello stesso tempo. La differenza è evidente a occhio nudo e apre il discorso su Weber–Fechner contro Stevens: due modelli diversi della stessa cosa, uno logaritmico e uno a potenza.

---

## 6. Due MOSFET quasi identici che si comportano in modo opposto

**Fisica — dissipazione, e perché leggere la scheda tecnica**

Il progetto iniziale prevedeva un IRF520; è stato sostituito con un AOD4184. Sembrano lo stesso componente: entrambi MOSFET a canale N, entrambi da decine di ampere.

La differenza sta in **una riga della scheda tecnica**: la tensione di pilotaggio a cui la resistenza di conduzione è specificata.

| | AOD4184 | IRF520 |
|---|---|---|
| $R_{DS(on)}$ a $V_{GS} = 10$ V | 5,0 mΩ | 270 mΩ |
| $R_{DS(on)}$ a $V_{GS} = 4{,}5$ V | **6,5 mΩ** | non specificata — il componente è solo parzialmente in conduzione |

Arduino pilota a 5 V. Con l'AOD4184, a 0,8 A per canale:

$$P = I^2 R = 0{,}8^2 \times 0{,}0065 = 4{,}2\ \text{mW}$$

Il componente resta freddo. Con l'IRF520 pilotato a 5 V la resistenza è molte volte più alta, la dissipazione cresce con il *quadrato* della corrente, e il modulo scalda fino a spegnersi.

**Laboratorio.** Misurare la caduta $V_{DS}$ ai capi dei due MOSFET a parità di corrente e ricavare $R_{DS(on)}$ da $R = V/I$. È una misura semplice, il risultato è netto, e insegna una cosa che vale più della formula: **due componenti dello stesso tipo non sono intercambiabili, e la differenza sta in una condizione di misura scritta in piccolo.**

---

## 7. Perché il valore medio deve restare fermo

**Fisica — valore medio, valore efficace, accoppiamento in alternata**

Il segnale che pilota il trasduttore è una sinusoide a 40 Hz la cui **ampiezza** varia con il respiro. La prima stesura del firmware la generava fra 0 e "ampiezza", il che faceva variare anche il **valore medio**.

Domanda: perché è un errore?

Perché una componente continua variabile, dopo l'amplificatore, sposta la bobina del trasduttore dalla posizione di riposo: la scalda e le toglie escursione utile. La correzione è generare la sinusoide **simmetrica attorno a metà scala**, così il valore medio resta fisso a 2,5 V e varia solo l'ampiezza alternata.

Occasione per distinguere con un caso concreto tre grandezze che gli studenti confondono: valor medio, ampiezza di picco e valore efficace. Con l'attenuatore del progetto:

| | Valore |
|---|---|
| Valor medio (continua, bloccata dal condensatore) | 0,228 V |
| Escursione picco-picco | 0,356 V |
| Valore efficace della sinusoide | $0{,}356/(2\sqrt{2}) = 0{,}126$ V |

E si misura con un multimetro da diciotto euro: la procedura a tre punti è in [WIRING.md](../hardware/WIRING.md).

---

## 8. Il limite fisico che il software non può aggirare

**Fisica — costanti di tempo meccaniche**

Nel repository ci sono due versioni del firmware. La `v1` pilota due motori a massa eccentrica; la `v2` due trasduttori a bobina mobile. Stesso codice, stesso obiettivo, esito opposto — ed è il confronto più formativo del progetto.

Un motore a massa eccentrica produce vibrazione **ruotando**: la frequenza è la velocità di rotazione, e il PWM ne cambia l'ampiezza, non la frequenza. La sua costante di tempo meccanica è di 20–50 ms. Un periodo da 40 Hz dura

$$T = \frac{1}{40} = 25\ \text{ms}$$

Il rotore non fa in tempo né ad accelerare né a fermarsi: quello che si percepisce è un ronzio a intensità pressoché costante.

Un trasduttore a bobina mobile invece **oscilla** attorno a una posizione di riposo: la sua frequenza è quella del segnale. Il Dayton BST-2 dichiara 10–80 Hz con risonanza a 30 Hz, quindi 40 Hz cade al centro della banda utile.

**La lezione generale**, che vale ben oltre l'elettronica: nessuna quantità di software corregge un attuatore scelto male. Prima si guarda la fisica del trasduttore, poi si scrive il codice.

---

## 9. Il cuore risponde al respiro, e si misura

**Trasversale con scienze — aritmia sinusale respiratoria**

In inspirazione il cuore accelera, in espirazione rallenta. È un fatto fisiologico ordinario, mediato dal tono vagale, e diventa più marcato quando il respiro è lento e regolare.

Il firmware confronta, a ogni battito, la direzione dell'intervallo R-R con la direzione del respiro, e accumula la concordanza in una media mobile esponenziale — la stessa struttura del punto 2. Il risultato pilota la saturazione del colore: cuore e respiro fuori fase fanno sbiancare la luce, la coerenza la satura.

È l'unica grandezza del sistema che usa entrambi i sensori insieme, ed è anche l'unica che non si può falsificare con la volontà. Si può respirare finto; non si può fingere la risposta del nodo seno-atriale.

**Discussione in classe:** cosa distingue una misura da un indicatore? L'indice di coerenza di questo progetto è un numero che si comporta ragionevolmente, ma non è tarato contro niente. Che cosa servirebbe per chiamarlo *misura*?

---

## 10. Costruire il proprio gruppo di controllo

**Metodo sperimentale — forse il percorso più utile di tutti**

Il dispositivo stimola. Nulla, in sé, dice se la stimolazione faccia qualcosa. È la differenza fra «l'ho costruito» e «ho verificato che funziona», e nei progetti didattici di elettronica quella differenza viene quasi sempre saltata.

Il firmware ha una **modalità sham**: un ponticello fra D4 e massa azzera la portante a 40 Hz e lascia identico tutto il resto — luce, suono, scivolamento, calcolo della coerenza, tempi di risposta. Chi la usa non può accorgersene.

Il ponticello non è un'opzione di compilazione di proposito: chi compila saprebbe sempre in quale condizione si trova, e il confronto non varrebbe niente. Con il ponticello la condizione la può impostare un'altra persona, oppure si tira a sorte annotando l'esito su un foglio da aprire solo alla fine.

Ogni sessione finisce in EEPROM con durata, coerenza media, BPM medio e il flag sham. Dopo venti sessioni si hanno due gruppi da confrontare.

**Da discutere in classe**

- Perché il confronto va fatto *in cieco*, e cosa cambierebbe se chi usa l'apparecchio sapesse in quale condizione si trova.
- Perché serve alternare le condizioni invece di fare prima dieci sessioni vere e poi dieci sham. *Deriva temporale: si migliora comunque nel respirare, e l'ordine confonderebbe l'effetto con l'allenamento.*
- Con venti sessioni e una differenza di pochi punti percentuali sulla coerenza, **si può concludere qualcosa?** Ingresso naturale alla variabilità campionaria e al perché una differenza osservata non basta da sola.
- L'indice di coerenza non è tarato contro nessuna misura di riferimento. Che cosa servirebbe per chiamarlo misura invece che indicatore?

C'è anche un banco di prova per il codice: con `INGRESSI_SINTETICI` il firmware genera respiro e battito, e nel battito simula un'aritmia sinusale **nota**. Con quell'ingresso l'indice di coerenza deve salire verso 1. Se non lo fa, il difetto è nel codice e non nella fisiologia — che è esattamente il modo di distinguere un errore di misura da un fenomeno.

---

## Nota di metodo

Il file [VERIFICATION.md](../hardware/VERIFICATION.md) elenca **quattro errori di progetto trovati rileggendo le schede tecniche**, di cui due avrebbero danneggiato hardware e uno avrebbe reso il dispositivo inutilizzabile senza che se ne capisse la ragione.

Vale la pena leggerlo con la classe. Il progetto sembrava finito e coerente tre volte prima di esserlo davvero, e ogni volta è stato un numero — non un'intuizione — a smentirlo.
