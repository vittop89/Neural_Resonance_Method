# Avvertenze legali e d'uso

## Non è un dispositivo medico

Questo progetto è un esercizio didattico di elettronica e trattamento del
segnale. Non è destinato a diagnosticare, trattare, curare o prevenire alcuna
malattia o condizione, e non va usato per monitoraggio clinico.

Il valore BPM calcolato è una stima ottica soggetta ad artefatti da movimento,
non una misura clinica. L'indice di coerenza cardiorespiratoria è un segnale di
riscontro, non un parametro diagnostico. Il watchdog a tre secondi protegge
dall'attuazione con sensore scollegato: non è un allarme sanitario.

## Controindicazione assoluta

**Chi porta un pacemaker, un defibrillatore impiantabile (ICD) o un loop
recorder non deve usare questo apparecchio.** La ragione, insieme a tutte le
altre avvertenze di sicurezza, è in [SAFETY.md](SAFETY.md), che va letto per
intero prima di costruire o usare l'apparecchio.

## Licenze

- **Firmware** (`firmware/`): licenza MIT, vedi [LICENSE](LICENSE).
- **Documentazione, schemi e distinta** (`docs/`, `hardware/`, `README.md`,
  `SAFETY.md`): licenza [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/).

## Nessuna garanzia

Il firmware non è mai stato compilato né collaudato su hardware fisico. I
dimensionamenti sono verificati contro le schede tecniche dei costruttori, non
contro misure di laboratorio. Chi costruisce l'apparecchio lo fa sotto la
propria responsabilità e dovrebbe verificare ogni valore prima di alimentare
il circuito.
