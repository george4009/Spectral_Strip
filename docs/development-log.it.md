> Historical development notes in Italian, kept for reference. See the English [README](../README.md) and [CHANGELOG](../CHANGELOG.md) for current documentation. File names and scripts mentioned below refer to the layout before 0.8.0.

# Verifiche — 28 settembre 2026

Ambiente: macOS Apple Silicon, Xcode 26.3 (17C529), JUCE locale 9.0.2.

- Generazione del progetto Xcode completata con CMake.
- Compilazione Debug arm64 riuscita: AU, VST3, Standalone e SpectralStripTests.
- La compilazione finale non ha riportato warning o errori.
- CTest: 1 suite eseguita, 1 superata, 0 fallimenti.
- Ricostruzione neutra stereo e latenza dry verificate a 44,1 / 48 / 96 / 192 kHz.
  Errore massimo assoluto della ricostruzione: 1,19209e-7.
- Stress numerico degli effetti a tutti e quattro i sample rate: nessun campione non finito.
- Freeze mono: il tono catturato continua dopo la cessazione dell'ingresso.
- Gate normale: attenua un tono sotto soglia; Gate invertito: conserva il tono.
- Delay: un burst emerge con il ritardo atteso e conserva la forma d'onda nella regione stabile.
- Standalone avviato; layout controllato visivamente, comprese le etichette della scala logaritmica.

Log riproducibili: `Builds/build-final.log` e `Builds/Testing/Temporary/LastTest.log`.

Non eseguiti: ascolto critico con materiale musicale, validazione auval/pluginval,
caricamento in DAW, test Intel, build Release e firma/notarizzazione per distribuzione.
Il test di stabilità numerica non sostituisce queste verifiche.

## Aggiornamento FFT Size

- Selettore 512 / 1024 / 2048 / 4096 / 8192 aggiunto e interfaccia controllata visivamente.
- AU, VST3 e Standalone ricompilati: `Builds/build-fft-size.log`.
- Test ampliati ricompilati: `Builds/build-fft-tests.log`.
- CTest superato (3,86 s): ricostruzione, latenza dry, isolamento stereo e invalidazione
  della memoria Delay a tutte le dimensioni e a quattro sample rate.
- Test del processor superati: cambi FFT con blocchi da 257 campioni, recupero del segnale
  dopo la dissolvenza, aggiornamento della latenza dichiarata, richiamo dello stato,
  sessioni precedenti prive del parametro e cambi rapidi durante bypass host.
- Rimane da verificare il comportamento della compensazione dinamica nella DAW scelta.

## Correzione click all'avvio — 0.1.1

- Gestiti avvio trasporto, discontinuità della timeline e reset host.
- Raccordo smoothstep di 5 ms sull'ingresso e sul residuo d'uscita; nessuna nuova latenza.
- Compilati AU, VST3, Standalone e test: `Builds/build-click.log`.
- CTest superato in 4,68 s, inclusa regressione con ingresso costante non nullo,
  tutte le dimensioni FFT, wet/dry, riavvio Play, seek e reset host.
- Audio Unit installata aggiornata a 0.1.1; hash identico al prodotto compilato e firma valida.
- Backup precedente: `Backups/20260928-235235/Spectral Strip.component`.
- auval passa, ma il registro Apple restituisce ancora i metadati della versione 0.1.0:
  `Builds/auval-click.log`. La rilettura della nuova versione da parte di Logic
  richiede la chiusura e riapertura dell'host. Nessuna sessione Logic è stata chiusa.
- Il click segnalato nella sessione reale non è stato ascoltato direttamente:
  resta da confermare la correzione sul materiale dell'utente dopo il riavvio.

## Filtro LOW/HIGH — 0.1.2

- Causa del mancato funzionamento in Logic: era installata la Audio Unit 0.1.1, priva del filtro.
- AU, VST3, Standalone e test compilati senza warning: `Builds/build-filter.log`.
- Nuovo test sul processor: LOW e HIGH, tramite i parametri, attenuano oltre 30 dB
  fuori banda con effetti spenti; banda passante e FILTER disattivato intatti.
- CTest superato (5,56 s): `Builds/build-filter-tests.log`.
- Audio Unit 0.1.2 installata; auval riuscito con versione 0.1.2: `Builds/auval-filter.log`.
- Backup precedente: `Backups/20260929-000829/Spectral Strip.component`.
- Da confermare all'ascolto in Logic.

## Miglioramenti effetti — 0.2.0

- Nuovi controlli: SHIMMER (Freeze), ATTACK, RELEASE, TILT (Gate), DAMPING (Delay).
- DISPERSION ora logaritmica; interpolazione del delay su ampiezza e fase.
- Interfaccia portata a 1040×800, valori con decimali adeguati: `Builds/ui-0.2.0.png`.
- AU, VST3, Standalone e test compilati senza warning: `Builds/build-effects.log`.
- CTest superato (7,6 s). Nuovi test: spaziatura per ottave di DISPERSION; ritardo di mezzo frame
  su un tono a 1 kHz senza perdita di livello (con il vecchio metodo circa -6 dB);
  DAMPING che accorcia le ripetizioni a 8 kHz e lascia intatte quelle a 150 Hz;
  ATTACK e RELEASE indipendenti; TILT che conserva un tono debole a 8 kHz;
  SHIMMER che modifica il freeze mantenendone il livello (rapporto di energia 0,99);
  sessioni precedenti riaperte con i valori che ne preservano il suono. Stress numerico esteso ai nuovi controlli.
- Audio Unit 0.2.0 installata; auval riuscito con versione 0.2.0: `Builds/auval-effects.log`.
- Backup precedente (0.1.2): `Backups/20260929-002055/Spectral Strip.component`.
- Da valutare all'ascolto: carattere di SHIMMER, range di DAMPING e default del Gate su materiale reale.

## Freeze più fluido — 0.2.1 (30 settembre 2026)

- SHIMMER usa offset di fase limitati, con obiettivi casuali ogni 150 ms e raccordo di 80 ms, al posto della precedente passeggiata casuale di fase per frame.
- Rimossa la compensazione fissa di guadagno della vecchia modulazione; ampiezza modulata più delicatamente.
- Test Freeze esteso a tutte le cinque FFT: energia relativa sul tono a 440 Hz tra 0,812 e 1,219 (circa ±1 dB). La soglia minima di differenza dal Freeze statico passa da 0,1 a 0,02 per riflettere il movimento intenzionalmente più delicato.
- Suite CTest superata in 8,08 s; build AU/VST3/Standalone e test senza warning/errori nei log build-freeze-motion.log e build-freeze-tests.log.
- AU installata: versione nel plist 0.2.1 e hash identico al compilato; firma valida. auval passa, ma restituisce ancora versione 0.2.0 dalla cache: Builds/auval-freeze-motion.log.
- Backup AU precedente: Backups/20260930-124001/Spectral Strip.component.
- Rimane da confermare all’ascolto in Logic il miglioramento timbrico su materiale musicale. Le sessioni con SHIMMER maggiore di zero cambiano carattere.

## Ridimensionamento — 0.2.2

- Maniglia JUCE in basso a destra; proporzioni 1040:800, limiti 780×600–1560×1200.
- Grafica, controlli e regioni di repaint scalano insieme; maniglia indipendente dalla scala.
- Provati riduzione al minimo e successivo ingrandimento nello Standalone: controlli e testi visibili, nessun taglio del layout.
- Compilati AU, VST3, Standalone e test; CTest superato in 8,02 s.
- AU 0.2.2 installata con firma valida e binario identico alla build. auval passa con metadati 0.2.1 ancora in cache; verifica del trascinamento dentro Logic da effettuare dopo riapertura.
- Backup: Backups/20260930-124432/Spectral Strip.component. Log: Builds/build-resize.log, Builds/auval-resize.log.

## Strip modulare — 0.3.0 (30 settembre 2026)

- Freeze/Smear, Gate, Spectral Delay e Filter inseribili/rimovibili e riordinabili dalla testata; un’istanza per tipo.
- Routing spettrale in ordine di strip, usando una sola FFT: Freeze cattura l’ingresso della propria posizione e Gate rileva il segnale che riceve dal modulo precedente.
- La catena è pubblicata atomicamente al processore audio e applicata al confine di un blocco con fade-out, reset delle memorie e warmup; nessuna allocazione o lock aggiunti nella callback. Il riordino azzera le code.
- Ordine e presenza salvati nello stato; fallback per sessioni precedenti: Freeze → Gate → Delay → Filter. Parametri e ID automazione conservati. La catena originale mantiene la precedente miscela ai bordi della banda. Modifiche strutturali notificate alla DAW.
- Rimuovendo Filter la banda comune si riapre; valori LOW/HIGH conservati per il reinserimento.
- Test: ordine Gate/Delay cambia l’energia della coda (10,3449 contro 0,00000191738); strip vuota trasparente anche con effetti abilitati; richiamo della catena; sanitizzazione di ID/duplicati; migrazione legacy; cambi di topologia durante playback con recupero di livello. Tutte le regressioni precedenti passano. CTest: 8,04 s.
- AU/VST3/Standalone e test compilati senza warning/errori: Builds/build-modular.log. Test: Builds/test-modular.log.
- Interfaccia verificata nello Standalone: rimozione Filter, reinserimento dal menu e trascinamento Filter al primo posto. Ridimensionamento proporzionale mantenuto.
- AU installata 0.3.0: plist verificato, binario identico alla build, firma valida. auval passa ma riporta metadati 0.2.2 ancora in cache: Builds/auval-modular.log.
- Backup precedente: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-131333/Spectral Strip.component.
- Da confermare in Logic dopo riapertura: interazione della strip, salvataggio nel progetto e ascolto dei cambi di catena.

## Filtro fisso e istanze multiple — 0.3.1 (30 settembre 2026)

- LOW/HIGH/FILTER ripristinati nella fascia fissa inferiore; filtro wet a valle della catena e attivo anche con strip vuota. Nessuna voce Filter nel selettore.
- Fino a otto istanze totali di Freeze/Smear, Gate e Delay, liberamente ripetibili. Scorrimento orizzontale con quattro schede visibili e scorrimento ai bordi durante il trascinamento.
- Identità di slot stabile indipendente dalla posizione; parametri APVTS distinti, Freeze/phase/magnitude, Gate envelope e storia Delay separati per slot e canale. FFT e latenza restano condivise.
- Storie Delay preallocate per tutti gli slot in prepareToPlay; parametri delle istanze letti tramite puntatori atomici precalcolati, senza costruzione di ID/allocazioni nella callback.
- Salvataggio catena e valori indipendenti; migrazione stati precedenti e 0.3.0 rimuovendo Filter dalla catena e conservando l’ordine degli effetti. ID originali Freeze/Gate/Delay mantenuti sui tre slot iniziali.
- CTest superato in 10,35 s: duplicati Gate con soglie opposte indipendenti; due Delay in serie sommano i rispettivi tempi e mantengono la forma d’onda; filtro fisso con strip vuota; otto Delay inseribili; valori 120/430 ms conservati dopo inversione della catena e richiamo; migrazione 0.3.0; regressioni precedenti.
- Interfaccia verificata: aggiunte due copie di Delay (cinque moduli totali), nuova copia portata a 430 ms e trascinata davanti al Gate mantenendo il valore, altre copie a 250 ms; scorrimento della strip al primo modulo e filtro sempre visibile.
- AU/VST3/Standalone e test compilati senza warning/errori: Builds/build-instances.log. Test: Builds/test-instances.log.
- AU installata 0.3.1: plist verificato, binario identico alla build, firma valida. auval passa con metadati 0.3.0 ancora in cache: Builds/auval-instances.log.
- Backup precedente: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-133313/Spectral Strip.component.
- Restano da valutare in Logic ascolto e carico CPU con molte istanze attive; i cambi strutturali mantengono il reset delle code e la breve dissolvenza.

## Spectral Blur — 0.4.0 (30 settembre 2026)

- Nuovo modulo Blur ripetibile e riordinabile: AMOUNT 0–1, WIDTH 20–4000 Hz, MIX 0–1, Enabled. Default 0,60 / 400 Hz / 1,00. Parametri distinti per slot, salvati e richiamati; parametri nuovi aggiunti dopo quelli esistenti per conservarne l’ordine. Migrazione del vecchio Filter mantenuta.
- Elaborazione per moduli sulla FFT condivisa: tre medie mobili sulla potenza spettrale, normalizzazione della banda LOW–HIGH e banco di fasi continuo per i bin diffusi. Complessità lineare nel numero di bin; nessuna allocazione o lock aggiunti nel render. Controlli raccordati, nessuna latenza supplementare.
- Test su tono a 1500 Hz: deviazione spettrale 62,4699 Hz con WIDTH 100 Hz, 857,457 Hz con WIDTH 1800 Hz; due copie da 100 Hz producono 86,8637 Hz. Rapporto di potenza del Blur largo 0,806679 rispetto al neutro: la normalizzazione spettrale non garantisce identico livello dopo la ricostruzione.
- Verificati Amount/Mix zero e bypass trasparenti, copie cumulative, valori finiti alla larghezza massima su tutte le cinque FFT, routing audio del processore e richiamo di due copie indipendenti dopo riordino. Regressioni Freeze/Gate/Delay/filtro e stati precedenti superate. CTest: 10,88 s, nessun fallimento.
- AU/VST3/Standalone e test compilati; log Builds/build-blur.log e Builds/test-blur.log. Nessun warning/errore nei log di compilazione verificati.
- Anteprima dell’editor verificata: due Blur, modifica indipendente di Width a 1200 Hz e Amount a 0,35; prima copia trascinata dopo Freeze conservando Width 1200 Hz. Filtro fisso e barra della strip visibili. Target opzionale SpectralStripPreview, senza apertura di hardware audio.
- Lo Standalone ha atteso in CoreAudio durante l’apertura del dispositivo, prima della creazione dell’editor; stack salvato in Builds/standalone-blur-sample.txt. Il processo di verifica è stato chiuso; l’anteprima ha permesso il controllo UI. Avvio Standalone e ascolto musicale nella DAW restano da confermare.
- AU 0.4.0 installata: versione plist verificata, binario identico alla build e firma valida. auval passa, ma riporta ancora metadati 0.3.1 dalla cache: Builds/auval-blur.log; questo non conferma la ricarica della 0.4.0 nell’host.
- Backup precedente: Backups/20260930-142443/Spectral Strip.component.

## Valori senza box e knob Delay — 0.4.1

- Rimossi sfondo e bordo dai valori di tutti i knob, compreso l’editor del valore; digitazione conservata.
- Ingranditi i cinque knob Delay riducendo il margine grafico e aumentando l’altezza delle righe; layout verificato nell’anteprima con Delay/Blur/Freeze/Gate.
- AU, VST3, Standalone e anteprima compilati: Builds/build-knobs.log. Modifica esclusivamente grafica.
- AU 0.4.1 installata, versione e identità del binario verificate. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-151855/Spectral Strip.component.

## Spazio indicatori Delay e intensità Blur — 0.4.2

- Margine dei knob Delay aumentato per contenere il thumb a mezzogiorno; verificato visivamente nell’anteprima, incluso Delay Mix 0,50.
- Rimossa la doppia attenuazione di AMOUNT: interpolazione unica verso lo spettro completamente diffuso. Zero e massimo conservano il comportamento precedente; valori intermedi più incisivi, anche negli stati esistenti.
- Nuovo test su differenza audio a AMOUNT 0,60 rispetto alla trasformazione massima, con tolleranza 0,55–0,65. Tutte le regressioni passano, CTest 11,25 s: Builds/test-blur-strength.log. Build: Builds/build-blur-strength.log.
- AU 0.4.2 installata, binario identico alla build e firma verificata. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-152409/Spectral Strip.component. Valutazione musicale del nuovo Amount da effettuare nella DAW.

## Chiusura del filtro — 0.4.3

- HIGH al minimo, LOW al massimo e limiti LOW/HIGH incrociati chiudono il wet completamente dopo il raccordo. Corretti il residuo nel bin DC e il riordino dei limiti che riapriva una banda.
- Test con componente continua e toni a 40/440 Hz, tutte le cinque FFT: chiusura superiore a 100 dB di attenuazione e successiva riapertura. Tutte le regressioni superate: CTest 11,48 s, Builds/test-filter-endpoints.log.
- AU/VST3/Standalone compilati senza warning/errori: Builds/build-filter-endpoints.log. AU 0.4.3 installata, versione, identità del binario e firma verificate. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-180752/Spectral Strip.component.
- La quota dry rimane volutamente non filtrata; verifica della chiusura con FILTER attivo e DRY/WET 1,00.

## Tremolo — 0.5.0

- Modulo tipo 5: RATE 0,10–20 Hz, DEPTH 0–1, SHAPE 0–1 da seno a impulsi arrotondati; Enabled raccordato. Default 4 Hz / 0,75 / 0. Parametri nuovi in coda, vecchi ID e migrazione conservati. Istanze duplicabili e riordinabili, richiamo indipendente.
- Modulazione per campione nel frame alla posizione del modulo (IFFT, inviluppo, FFT), banda comune e filtro finale conservati. Stereo in fase, memoria preallocata, nessun lock o latenza aggiuntiva. Due trasformazioni aggiuntive per Tremolo attivo/canale: costo CPU da valutare nella DAW con molte copie. Rate libero, senza BPM sync.
- Test: sinusoide ricostruita contro formula analitica, Depth zero, bypass, due copie in serie, coerenza stereo e impulsi a 20 Hz su tutte le cinque FFT. Richiamo di due Rate indipendenti 2/7 Hz e Shape 0,8, parametri collegati all’audio del processore. Regressioni complete superate: CTest 13,17 s.
- AU/VST3/Standalone, test e anteprima compilati senza warning/errori: Builds/build-tremolo.log e Builds/test-tremolo.log. UI verificata nell’anteprima: Tremolo visibile, Rate 6,25 Hz e Shape 0,65 impostati correttamente, valori senza box.
- AU installata 0.5.0, plist e identità binario verificati, firma valida. Log validazione host: Builds/auval-tremolo.log. Backup precedente: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-181712/Spectral Strip.component.
- auval superato, con versione 0.4.3 ancora nei metadati in cache; la ricarica della 0.5.0 nell’host va confermata dopo riapertura. Ascolto musicale da effettuare nella DAW.

## Controllo generale — 0.5.1

- Ispezionati motore DSP, processore, gestione stato e controlli UI. Due regressioni riprodotte prima della correzione: Builds/test-audit-repro.log.
- Filtro chiuso all’avvio/reset: il gain iniziale a uno produceva un picco 0,257972 con ingresso 0,3. Il primo frame adotta ora subito la risposta richiesta; picco corretto zero su tutte le FFT. Le variazioni durante playback mantengono il raccordo.
- Richiamo stato con identica catena/FFT: Freeze continuava a riprodurre una vecchia cattura. Una revisione atomica richiede ora il reset sul thread audio dopo fade-out, anche senza variazioni topologiche; la misura di uscita successiva al richiamo passa da 8,14413 a zero nel test. Memorie Delay e fase Tremolo vengono anch’esse reimpostate, come negli altri reset.
- Nuova prova di otto moduli misti, automazioni alternate ogni 1024 campioni, feedback 0,92, estremi Blur/Tremolo e LOW/HIGH, 44,1/96 kHz e FFT 512/8192: tutti i campioni finiti, nessun segnale nel canale destro silenzioso, picchi 0,041–0,089 con sorgente rumore ±0,06. Questo test verifica stabilità e isolamento, non l’assenza percettiva di ogni artefatto sotto automazione.
- Tempi del renderer offline nei quattro casi: 24–49% della durata audio; comprendono generazione della sorgente e controlli del test. Non sono una misura del carico CPU nella DAW né una garanzia sulle scadenze di ogni buffer.
- Regressioni complete superate: 16,68 s, Builds/test-audit.log e Builds/Testing/Temporary/LastTest.log. Ricostruzione neutra, bypass, avvio/seek/reset, filtro, ritardi frazionari, Freeze, Gate, Blur, Tremolo, copie e migrazioni.
- Compilati AU/VST3/Standalone, test e anteprima in Release, senza warning/errori: Builds/build-audit.log. Aggiunti tooltip per il filtro sulla quota wet e il minimo Delay di un hop FFT.
- Installata AU Release 0.5.1: versione plist, firma e identità del binario verificate. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20260930-184833/Spectral Strip.component. Log auval: Builds/auval-audit.log.
- Limiti residui: il timbro su materiale musicale e il carico con molte istanze in Logic richiedono ascolto/prova nella DAW; i cambi strutturali e il richiamo stato azzerano le code con una breve dissolvenza.
- auval superato; i metadati host riportano ancora 0.5.0 in cache. Questo esito non conferma la ricarica della 0.5.1 nella DAW, da riaprire.

## Ricontrollo funzionale — 1 ottobre 2026, versione 0.5.1

- Ricompilazione Release AU/VST3/Standalone e suite completa riuscita senza warning/errori (Builds/build-functional-check.log). Nessuna modifica al DSP o alla versione installata.
- Aggiunto test del parametro Shimmer nel processore completo, slot originale 1 e copia slot 4: con FREEZE spento differenza audio zero; con FREEZE acceso differenza relativa di energia 0,164058 e 0,0987597. Verifica della variazione durante playback, oltre ai precedenti test del solo motore.
- Suite ampliata superata in 17,54 s, zero fallimenti: Builds/test-functional-check.log. Include filtro, Freeze/Smear, Gate, Delay, Blur, Tremolo, copie, bypass, transizioni, richiamo stato e stress di otto moduli.
- AU installata identica alla build Release (SHA-256 76427434e1210a2769532585f5dceb2c689d61565a7542a80bc55fb1cfb6ddd7), firma valida. auval riconosce ora esplicitamente 0.5.1 e supera la validazione: Builds/auval-functional-check.log. Il primo tentativo nel sandbox non vedeva il registro AU; verifica riuscita con accesso al registro host.
- Nessun nuovo difetto riprodotto nei casi verificati. Questo controllo è numerico e tramite validatore AU: non sostituisce ascolto musicale e prova di un progetto reale nella DAW.

## Shimmer +12 semitoni — 0.6.0 (1 ottobre 2026)

- Sostituito il movimento casuale del Freeze con voce trasposta all’ottava superiore: inviluppo spettrale interpolato, fasi legate ai picchi locali e avanzamento raddoppiato. Componenti con frequenza trasposta oltre Nyquist escluse. Mix raccordato a 80 ms; al massimo coefficienti 0,707 per originale/ottava. Nessuna latenza aggiuntiva.
- Shimmer zero mantiene il Freeze originale; sul segnale non congelato non interviene. Parametro, copie e salvataggio conservati; gli stati con Shimmer non nullo cambiano timbro intenzionalmente. Tooltip e README aggiornati.
- Test diretto 440 → 880 Hz su tutte le FFT: ampiezza dell’ottava pari a 0,51–0,57 della fondamentale di riferimento; rapporto di energia totale 0,76–0,83 rispetto al Freeze senza Shimmer. Le differenze di ricostruzione impediscono identico livello effettivo delle due voci.
- Suite completa superata in 19,04 s, compresi Shimmer tramite processore e copia slot 4, bypass fuori Freeze, richiamo e stress: Builds/test-octave.log. Compilazione Release AU/VST3/Standalone e test senza warning/errori: Builds/build-octave.log.
- AU Release 0.6.0 installata: plist, identità binario e firma verificati. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20261001-140626/Spectral Strip.component. Da valutare il risultato musicale all’ascolto nella DAW.

## Shimmer Reverb autonomo — 0.7.0 (1 ottobre 2026)

- Nuovo modulo tipo 6, duplicabile e riordinabile: Enabled, Decay 0,5–20 s, Size, Shimmer, Tone e Mix. Default 6 / 0,60 / 0,45 / 0,55 / 0,35. Parametri aggiunti in coda, valori e ordine salvati per istanza.
- Rimosso lo Shimmer dal Freeze e dal suo DSP. I parametri storici restano registrati per non spostare le automazioni, ma sono inerti: verificato su tutte le FFT e sui parametri del processore originale/copia. Nessun inserimento automatico nei progetti vecchi.
- Motore temporale: otto linee FDN con matrice Hadamard normalizzata, quattro diffusori allpass, modulazione lenta, smorzamento, filtro passa-alto nel ritorno trasposto, pitch shifting granulare a due testine +12 semitoni nel feedback e contenimento morbido dei valori interni eccessivi. Due reti con tempi differenti per i canali. Storage preallocato, reset tramite invalidazione delle linee.
- Ponte FFT/tempo: overlap-add dei frame in ingresso, consumo dei soli hop definitivi, rianalisi dell’uscita. Ogni copia aggiunge N−hop campioni di latenza, comunicata all’host; dry compensato anche in bypass e con banda selezionata. Corretto il mix iniziale per evitare residui su Mix zero. Freeze dopo il riverbero attende il riempimento del ponte.
- Test: ricostruzione neutra Mix zero e bypass con una/due copie su tutte le FFT, errore massimo < 1,7e-7; latenza esatta dry/wet. Coda lunga/short con ingresso 440 Hz: energia tardiva 23,323 contro 9,93e-9. Misura componente 880 Hz nella coda: 32,38 con Shimmer contro 0,0696 senza. Valori finiti nel test, coda sostenuta dopo ingresso silenzioso.
- Verificati reset delle memorie, Freeze dopo il ponte, inserimento di copie, richiamo ordine/Decay indipendente, latenza host e recupero dopo rimozione. Suite completa superata in 23,05 s: Builds/test-shimmer-reverb.log.
- Compilazione Release AU/VST3/Standalone, test e anteprima riuscita senza warning/errori nella build finale: Builds/build-shimmer-reverb-final.log. UI verificata: Decay 8 s, Shimmer 0,70, Freeze con solo Smear; footer 74,7 ms a 48 kHz/FFT 2048 e una copia.
- AU 0.7.0 installata, plist e identità binario verificati, firma valida. Backup: /Users/giorgio/Desktop/SpectralStrip/Backups/20261001-143333/Spectral Strip.component.
- Limiti: Decay nominale dipende anche da Tone/Shimmer; Size in movimento può produrre variazioni di intonazione. Latenza maggiore soprattutto a FFT grandi. La qualità musicale rispetto al riferimento Valhalla richiede ascolto, non è dimostrata dai test funzionali.
- auval superato: Builds/auval-shimmer-reverb.log. I metadati mostrano ancora 0.6.0 in cache; la versione 0.7.0 del bundle è verificata direttamente, la DAW va riaperta per ricaricarla.

## Correzione coda Shimmer — 0.7.1 (1 ottobre 2026)

- Riprodotto un difetto nella 0.7.0: Shimmer al massimo sottraeva il 45% del feedback a ogni giro e lo sostituiva con un ritorno a rango ridotto. Con burst 440 Hz, Decay 8 s e Tone 0,8, energia della coda fra 2 e 4 s: 5,913 senza Shimmer, 8,96e-10 con Shimmer massimo. Il precedente test dell’ottava non controllava questo collasso.
- Ritorno trasposto ora dosato rispetto alla perdita prevista da Decay (20% di tale perdita, massimo 0,12), con due direzioni ortogonali. Aggiunte due voci +12 semitoni nell’eccitazione della rete, con grani 43,7/61,3 ms per ridurre le cancellazioni comuni. Shimmer zero conserva il percorso del riverbero originale. Nessuna variazione di parametri, default o latenza.
- Stesso caso dopo la correzione: energia tardiva 3,205, pari al 54,2% del riverbero senza Shimmer; componente a 880 Hz nel test preesistente 690,929 contro 32,381 nella 0.7.0. Sono misure di energia, non dB o giudizi musicali.
- Aggiunte regressioni su 220/440/450 Hz a 44,1/48/96 kHz, entrambe le reti dei canali: energia tardiva con Shimmer massimo pari al 54–77% del riferimento. Verificati ottava e coda attraverso il ponte FFT, automazione Size/Decay, stabilità a 44,1/192 kHz e reset di tutte le memorie del pitch.
- Suite completa superata in 40,19 s, zero fallimenti: Builds/test-shimmer-fix.log, dettagli in Builds/Testing/Temporary/LastTest.log. Compilazione Release AU/VST3/Standalone, test e preview senza warning/errori: Builds/build-shimmer-fix.log.
- AU 0.7.1 installata: versione del bundle e identità binario verificate, firma valida, auval superato (Builds/auval-shimmer-fix.log). Backup AU: Backups/20261001-190836/Spectral Strip.component; backup sorgente e test: Backups/pre-shimmer-tail-fix.
- Questi controlli dimostrano la correzione della coda e la presenza dell’ottava, non equivalenza sonora a Valhalla. Rimane da valutare il risultato su materiale musicale nella DAW dopo averla riaperta per caricare la nuova AU.
- Il registro auval riporta ancora 0.7.0 nei metadati in cache; la versione 0.7.1 è verificata nel plist installato e il binario coincide con la nuova build. La validazione non prova che una DAW già aperta abbia ricaricato la nuova versione.

## Smear: fase della coda — 0.7.2 (1 ottobre 2026)

- Riprodotto difetto della 0.7.1: le ampiezze smussate restavano non nulle sul silenzio ma la fase veniva ricavata da arg(0). La coda diventava periodica rispetto all’hop FFT anziché conservare l’intonazione. Test 440 Hz/48 kHz, una nota di 1 s, analisi della coda fra 1,5 e 2 s: frazione di energia coerente a 440 Hz 0,000108 / 0,000469 / 0,00558 per FFT 512/2048/8192. Regressione fallita prima della correzione: Builds/test-smear-before.log.
- Aggiunta memoria di fase e avanzamento per bin, con stima smussata a 50 ms sui frame di ampiezza stabile e prosecuzione quando l’ingresso è insufficiente. Evita di aggiornare la frequenza con il transitorio di spegnimento. Smear a zero conserva la fase originale. Freeze con Smear cattura anche fase/avanzamento della coda residua, non quelli dell’ingresso silenzioso. Tutte le nuove memorie si azzerano al reset.
- Test mirati superati su tutte le FFT: frazione coerente a 440 Hz nella coda 0,991 / 0,958 / 0,907 / 0,987 / 0,980 per FFT 512/1024/2048/4096/8192. Verificati cattura Freeze durante la coda, reset, controlli reali via processore negli slot 1 e 4, valori 0/0,5/1, bypass e canale destro silenzioso. Cambiare Smear dopo una cattura non modifica il Freeze già sostenuto, come previsto.
- Tooltip e README chiariscono che Smear è una media delle ampiezze precedente al Freeze, con costante di tempo massima 1,5 s (non durata massima della coda). Parametri, default, ordine e latenza invariati. Backup sorgente/test: Backups/pre-smear-phase-fix. Runner test: aggiunta opzione --audit per regressioni mirate.
- Suite completa superata in 43,27 s, nessun fallimento (Builds/test-smear-fix.log). Release AU/VST3/Standalone, test e preview compilati senza warning/errori (Builds/build-smear-fix.log).
- AU 0.7.2 installata: plist e identità binario verificati, firma valida, auval superato (Builds/auval-smear-fix.log). Backup AU: Backups/20261001-202501/Spectral Strip.component. Il validatore può riportare la precedente versione nei metadati in cache: occorre riaprire la DAW e verificare v0.7.2 nel footer per caricare la nuova build.
- Verifiche numeriche e funzionali: non sostituiscono una valutazione musicale su materiale reale.

## Tremolo Shape più evidente — 0.7.3 (1 ottobre 2026)

- Il parametro era collegato al DSP: la precedente curva passava da sinusoide a tanh, miscelando nuovamente il risultato con la sinusoide. Il duty cycle restava al 50% lungo tutta la corsa. L’intervento risponde alla differenza poco percepibile segnalata dall’utente; non era un comando scollegato.
- Nuova curva a coseno raccordato: Shape 0 è la sinusoide originale; da 0 a 0,5 le transizioni si restringono fino a un’onda quadra arrotondata; da 0,5 a 1 si restringe anche il duty cycle, dal 50% al 12,5%. Edge minimo 2 ms, gain compreso fra 1−Depth e 1, nessuna compensazione artificiale del livello medio. Rate, latenza, parametri e default invariati.
- Aggiunte verifiche dell’inviluppo su Shape 0/0,25/0,5/0,75/1 per tutte le FFT: durata sopra metà ampiezza, tempo nelle transizioni, differenza RMS fra posizioni successive, limiti del guadagno e coerenza stereo. Restano attivi i test preesistenti di sinusoide, copie seriali, bypass, Depth zero e pulsazione a 20 Hz su tutte le FFT.
- Tooltip e README aggiornati. I preset con Shape non nullo cambiano carattere intenzionalmente. Backup sorgenti/test: Backups/pre-tremolo-shape-fix.
- Suite completa superata in 46,88 s senza fallimenti: Builds/test-shape-fix.log. Build Release AU/VST3/Standalone, test e preview senza warning/errori: Builds/build-shape-fix.log. Inviluppi misurati e risultati per FFT in Builds/Testing/Temporary/LastTest.log.
- AU 0.7.3 installata, versione plist e identità binario verificate, firma valida e auval superato: Builds/auval-shape-fix.log. Backup AU: Backups/20261001-203304/Spectral Strip.component. Il registro AU può mostrare la precedente versione in cache: riaprire la DAW e controllare v0.7.3 nel footer. Il gradimento musicale richiede ascolto nella DAW.

## Freeze: fedeltà del timbro — 0.7.4 (2 ottobre 2026)

- Segnalazione: timbro congelato metallico o diverso dall’ingresso. Riprodotti battimenti artificiali su una nota di 220 Hz con quattro armoniche, catturata dopo 1 s. Con FFT 1024 la variazione di energia fra finestre di 50 ms era 65,4% e la frazione coerente sulle armoniche originali 72,1% (Builds/test-freeze-before.log).
- Aggiunto raggruppamento dei bin attorno ai picchi dello spettro catturato: ogni regione conserva le fasi relative iniziali e usa lo stesso avanzamento del picco. Nessuna modifica alle ampiezze catturate. Soglia picchi a −60 dB rispetto al massimo, DC/Nyquist esclusi dal raggruppamento. Eseguito solo al momento della cattura, con storage sullo stack, senza allocazioni.
- Stima di frequenza del Freeze raccordata a 20 ms per ridurre l’errore del singolo frame. Con Smear si usa ancora la stima della coda, ora con la stessa coerenza di fase fra bin vicini. Nessun parametro o ritardo aggiunto; reset azzera la nuova memoria della stima.
- Test mirato a 48 kHz: FFT 1024, frazione coerente 92,0%, variazione d’energia 0,217%, energia media 0,010242 contro riferimento teorico 0,01025. FFT 2048/8192: frazione coerente >99,9%. Queste misure descrivono il segnale sintetico, non equivalenza percettiva su qualsiasi materiale.
- Test esteso a tutte le FFT a 48 kHz e FFT 2048 a 44,1/96 kHz. Regressioni Smear/cattura sulla coda, reset, bypass, stereo e copie incluse. Backup sorgente/test: Backups/pre-freeze-refine.
- Suite completa superata in 48,83 s (Builds/test-freeze-fix.log), build Release AU/VST3/Standalone, test e preview senza warning/errori (Builds/build-freeze-fix.log).
- AU 0.7.4 installata, versione e identità binario verificate, firma valida e auval superato (Builds/auval-freeze-fix.log). Backup AU: Backups/20261002-174031/Spectral Strip.component. Registro AU soggetto a metadati precedenti in cache: riaprire la DAW e verificare v0.7.4 nel footer. Ascolto musicale nella DAW ancora da valutare dall’utente.


## Controllo generale e banda Smear — 0.7.5 (8 ottobre 2026)

- Verificata prima la base ripristinata 0.7.3: build completa riuscita, suite CTest superata in 47,72 s, firma AU valida e auval superato. Il Freeze conserva quell’algoritmo; la modifica di fase della 0.7.4 resta annullata.
- Corretto il trattamento degli estremi LOW/HIGH: a 20 Hz / 20 kHz gli effetti usano una maschera completamente aperta, coerente con il filtro. Prima alcune componenti dei transienti restavano dirette anche con Smear massimo e mix completamente wet. Le bande intermedie mantengono la selezione precedente.
- Aggiunti test Smear su attacchi isolati e ripetuti, sia a banda larga sia con rumore filtrato a 4 kHz. Nei colpi ripetuti a banda larga il picco con Smear massimo scende da 0,391 prima della correzione a 0,0615; senza Smear è 0,490. Nel caso filtrato i picchi Smear 0 / 0,5 / 1 sono 0,302 / 0,0364 / 0,0281, e l’energia dei primi 30 ms scende da 26,58 a 0,262. Queste misure verificano l’attenuazione degli attacchi sintetici, non sostituiscono un ascolto musicale in Logic.
- Build Release AU, VST3, Standalone, test e preview riuscita senza warning/errori. Suite completa superata in 49,15 s: tutti gli effetti, ordine e copie dei moduli, filtro, automazione, richiamo stato, reset, stereo e latenza alle dimensioni FFT e frequenze di campionamento previste.
- Verifica UI della base 0.7.3: aggiunta/rimozione di una quinta istanza Freeze, scorrimento della strip, indipendenza dei valori Smear fra copie e azionamento Freeze/Shape. Ridimensionamento e trascinamento dell’ordine non ricontrollati in questa sessione.
- AU 0.7.5 installata: versione del bundle e identità del binario con la build verificate; firma valida e AU VALIDATION SUCCEEDED. Backup precedente: Backups/20261008-013802/Spectral Strip.component. Log: Builds/build-general-smear-fix.log, Builds/test-general-smear-fix.log, Builds/Testing/Temporary/LastTest.log, Builds/auval-general-smear-fix.log. Riaprire Logic e verificare v0.7.5 nel footer per caricare il nuovo binario.

## Controllo funzionalità e ottimizzazione — 0.7.6 (10 ottobre 2026)

- Revisione del processore, DSP e interfaccia: allocazioni dei buffer in prepare, piani FFT precalcolati, parametri atomici, smoothing, gestione transport e richiamo stato. Nessun lock o nuova allocazione esplicita introdotto nel render.
- Suite della build precedente superata (62 messaggi PASS); build aggiornata Release AU/VST3/Standalone, test e preview riuscita senza warning/errori del compilatore. Suite finale CTest superata: 1/1, 47,13 s. Copertura esistente: tutti gli effetti, copie/ordine dei moduli, filtro/banda, Smear, automazione, stato legacy, reset/seek, bypass, isolamento stereo, latenza e FFT a più sample rate. Log: Builds/build-review-final-20261010.log, Builds/test-review-final-20261010.log e Builds/Review20261010/Testing/Temporary/LastTest.log.
- Ottimizzazioni: indice dry e latenza condivisi fra canali; riuso della fase già calcolata in Freeze/Smear. Nei buffer del riverbero, sostituiti modulo intero e doppia lettura con wrapping condizionale e riuso del campione. Letture valide limitate alla capacità del ring. Confronto del riverbero vecchio/nuovo bit-identico su 8 milioni di campioni, 44,1/48/96/192 kHz, entrambi i canali, variazioni parametri, silenzio, wrapping e reset. Log: Builds/reverb-equivalence-20261010.log; sorgente confronto e backup in Backups/pre-review-20261010.
- Benchmark --benchmark aggiunto al runner: 3 s di audio stereo sintetico a 48 kHz per FFT 512/1024/2048/4096/8192. Scenario 0: Freeze/Smear + Gate + Delay; scenario 1: sei effetti diversi; scenario 2: otto Shimmer Reverb. Preparazione esclusa dal cronometraggio; generazione segnale, controllo finitezza e checksum inclusi. Misure iniziali isolate in Builds/benchmark-review-isolated-20261010.log; finali, senza suite/build concorrenti, in Builds/benchmark-review-final-20261010.log. A FFT 2048: rapporto tempo DSP/audio 10,38 -> 10,23%, 35,20 -> 32,63%, 148,46 -> 131,04% rispettivamente. Singole misure indicative, senza analisi statistica; non sono misure della DAW o delle deadline dei callback. Otto riverberi restano oltre il tempo reale su questo Mac.
- Preview 0.7.6 avviata e layout controllato visivamente; verificati toggle Freeze e modifica Smear a 0,75 via UI. Aggiunta, trascinamento e ridimensionamento non riverificati manualmente in questa sessione; gestione catena/copertura DSP verificata dalla suite. Launch Services non ha aperto la preview: avvio diretto dell'eseguibile riuscito.
- CMake e JUCE mancanti nei percorsi originari: recuperati CMake 4.4.4 e JUCE tag 9.0.2, commit 72782788ce18c2d4d760b28e0921d6ffc6431102. Conservati in Builds/Dependencies; Generate Xcode.command usa il fallback locale. Build verificata in Builds/Review20261010. Nessun commit creato; modifiche locali preesistenti preservate.
- Installata AU arm64 0.7.6 in /Users/giorgio/Library/Audio/Plug-Ins/Components/Spectral Strip.component. Cartella Components inizialmente vuota, quindi nessuna AU precedente da salvare. Verificati plist, identità binaria con build e firma ad hoc valida. Primo auval non trovava il componente; aggiornato il registro riavviando AudioComponentRegistrar dell'utente, poi auval -v aufx SpSt GiAu -strict superato con versione rilevata 0.7.6: AU VALIDATION SUCCEEDED (Builds/auval-review-20261010.log).
- Riaprire la DAW e controllare v0.7.6 nel footer. Ascolto musicale e caricamento in Logic non verificati; validazione effettuata con auval.

## Ottimizzazione generale — 0.7.7 (12 ottobre 2026)

- Trovato un errore di misura nella 0.7.6: SpectralStripTests e la preview erano compilati a -O0 anche in Release, perché non collegavano juce_recommended_config_flags; il plugin era già a -O3. Corretto in CMakeLists.txt; verificato -O3 nei file di argomenti Xcode del target test. I numeri del benchmark 0.7.6 vanno quindi letti come misure non ottimizzate.
- Base 0.7.6 a -O3, FFT 2048: 3,30 / 6,21 / 15,31% (Builds/Optimization20261012/benchmark-baseline-O3.log), riconfermata a 3,27 / 6,16 / 15,17% a fine sessione. Profili con sample per scenario in Builds/Optimization20261012/profile-*.txt: dominavano atan2f, remainderf, sincosf, logaritmi per bin e, nel riverbero, sinf/cosf per campione.
- Ottimizzazioni senza cambiamenti audio: tabelle per FFT/sample rate di frequenza, avanzamento di fase atteso, ottave del tilt Gate, damping Delay e indice del display; maschera di banda, filtro e dispersione Delay calcolati una volta per hop, condivisi fra canali e moduli; wrapping della fase esatto in float/double al posto di std::remainder; seconda polar del Freeze saltata con freezeBlend esattamente zero; azzeramento dei buffer limitato a 2×FFT attiva; indici circolari con maschera; costanti per campione del riverbero precalcolate in prepare.
- Equivalenza: nuovo `--dump` del runner scrive wet/dry grezzi di 54 render (44,1/48/96 kHz, FFT 512/2048/8192, sei catene fra cui tutti gli effetti, otto riverberi e ordini misti), con automazione a gradini di banda, filtro on/off, Freeze, Smear, Gate inverso, Delay, Blur, Tremolo e Reverb. Uscita identica byte per byte alla base (cmp) dopo ogni passo e nella build Xcode finale. Riferimento: Builds/Optimization20261012/baseline.dump e baseline-SpectralStripTests.
- Il wrapping della fase è confrontato con std::remainder su tutti i 2.483.027.968 float positivi e negativi con modulo inferiore a 2^21, segno dello zero incluso: 0 differenze (wrapcheck.cpp, wrapcheck.log). Una prima versione differiva solo a −2π (+0 invece di −0), corretta prima dell'adozione.
- Risultato a FFT 2048, 48 kHz stereo: 2,00 / 4,42 / 13,42% nella build Xcode finale (benchmark-xcode-077.log), cioè circa −39%, −28% e −12%. Misure singole, indicative, non deadline dei callback nella DAW.
- Editor: spettro ridisegnato solo se un bin si muove più di 0,01 dB o cambia la banda LOW/HIGH; piè di pagina solo quando cambiano FFT, latenza o sample rate. Non verificato visivamente in questa sessione.
- Build Xcode Release ALL_BUILD riuscita senza warning/errori del compilatore (xcode-build-077.log); anche build Makefiles Release di tutti i target senza warning. Suite completa: 62 PASS, 6,4 s invece di circa 47 s grazie a -O3. AU prodotta con CFBundleShortVersionString 0.7.7, non installata. Backup sorgenti: Backups/pre-optimization-20261012.

## Venti moduli — 0.7.8 (12 ottobre 2026)

- maxModules portato da 8 a 20. Il formato storico mette tipo e slot in un byte e ne impacca otto in un int64: gli slot 16–20 non ci stanno. Gli slot 1–15 restano nel formato precedente; dal 16 in poi la voce è (tipo << 8) | (slot + 1), sempre maggiore di 255, così i due formati non si confondono.
- La catena pubblicata al thread audio non è più un solo atomico. Un contatore pari/dispari pubblica l'array: il render legge una copia coerente e la applica solo al termine della dissolvenza, come prima.
- Il salvataggio usa la proprietà moduleChain, elenco separato da virgole. In lettura, moduleChain ha precedenza; altrimenti si legge instanceChain (otto byte) o il chainCode della 0.3. Le sessioni vecchie non vengono riscritte finché non si salva di nuovo.
- L'interfaccia continua a mostrarne quattro; lo scorrimento usa già il numero di moduli presenti.
- Test: richiamo della catena a otto moduli dal formato int64; inserimento fino al ventesimo Delay con slot indipendente e rifiuto del ventunesimo; venti Gate in serie, l'ultimo dei quali chiude davvero la banda. La memoria di ogni slot, compreso il riverbero, è allocata in prepare anche se il modulo non è in uso.

## Automazione dei moduli in uso — 0.7.10 (12 ottobre 2026)

- I parametri di uno slot sono automatizzabili solo mentre quello slot è nella strip. LOW, HIGH, FILTER, mix, output e FFT Size restano sempre automatizzabili. Gli ID e i valori degli slot non in uso restano nel salvataggio: togliere un modulo lo nasconde dall'elenco, rimetterlo nello stesso slot riallaccia le lane.
- L'AU segnala i parametri nascosti come NonRealTime e, a ogni cambio di catena, chiede all'host di rileggere l'elenco. In VST3 il flag viene letto alla creazione del controller, quindi un host già aperto lo aggiorna al ricaricamento del plugin.
- Test: con Freeze e Delay in strip, smear e delayMs sono automatizzabili e threshold no; il richiamo della sessione aggiorna la stessa visibilità; aggiungere Blur espone il suo amount; svuotare la strip nasconde smear e lascia mix automatizzabile. Suite completa passata nella build Makefiles Release.
- Logic ignora il flag NonRealTime e mostra ogni ID restituito da GetParameterList. La 0.7.10 quindi lasciava visibili anche i moduli assenti.

## Elenco automazione di Logic — 0.7.11 (12 ottobre 2026)

- L'AU pubblica a Logic solo i parametri con isAutomatable() vero, e un parametro lo è solo se quello slot contiene proprio quell'effetto. Freeze non pubblica i knob di Gate, Blur o Reverb dello stesso slot. Il wrapper JUCE è modificato in juce_audio_plugin_client_AU_1.mm; la patch è in patches/juce-au-automatable-parameter-list.patch e Generate Xcode.command la riapplica se manca. CMake si ferma se il sorgente JUCE non la contiene.
- A ogni cambio di catena la cache dell'elenco viene svuotata e l'host riceve di nuovo ParameterList e ParameterInfo. Gli ID restano quelli di sempre, così una lane si riallaccia quando il modulo torna nello stesso slot. Logic conserva comunque l'elenco letto all'inserimento: togliere e rimettere il plugin è il modo per vederlo aggiornato.

## Memoria dei moduli assenti — 0.7.12 (12 ottobre 2026)

- Storia del Delay e linee dello Shimmer Reverb si allocano solo per gli slot che contengono quell'effetto, in prepareToPlay e prima di pubblicare una catena nuova. Gli altri effetti non le usano. Uno slot già in lettura dal thread audio non viene riallocato.
- Il tooltip generico di SHIMMER, rimasto dal Freeze rimosso, non è più assegnato. Il knob del riverbero tiene la descrizione della voce a +12 semitoni nel feedback.

## Nessuna allocazione nel callback — 0.7.13 (16 ottobre 2026)

- Il callback audio non alloca più la storia del Delay né le linee del riverbero. prepareToPlay e setChain le preparano prima che la catena diventi attiva. I test del motore, che chiamano process senza passare dal processore, tengono l'allocazione con SPECTRAL_ALLOW_PROCESS_ALLOCATION. Un Delay o un riverbero senza buffer fa scattare un jassert.

## Nomi dei moduli — 0.7.14 (23 ottobre 2026)

- In menu, testata e nomi di automazione, Spectral Delay si chiama Delay e Shimmer Reverb si chiama Shimmer. Gli ID dei parametri non cambiano.


## Corruzione memoria durante aggiunta moduli — 0.7.15 (23 ottobre 2026)

- Riscontro reale: AUHostingServiceXPC_arrow-2026-10-23-003014.ips, AU 0.7.14, abort del malloc per corruzione heap durante il disegno. Il test Tests/AUParameterStress.mm riproduce un abort per corruzione heap sulla 0.7.14 installata (AUParameterStress-2026-10-23-003734.ips).
- Causa riprodotta: la patch del wrapper AU modificava cachedParameterList dal message thread mentre il dispatch AU ne leggeva dimensione/contenuto sotto un altro lock. La lista poteva crescere fra il dimensionamento del buffer e la copia. Ora il message thread pubblica soltanto un flag atomico; la lista viene ricostruita esclusivamente nella richiesta della dimensione, sotto il lock di istanza del SDK. La copia usa la stessa lista e non la ricostruisce. Il filtro per effetto/slot resta attivo.
- Secondo difetto corretto: allocateFor da setChain poteva concorrere con selectSize/reset/prepare sul motore condiviso, anche per slot non ancora attivi (reset tocca tutti gli slot). Un mutex serializza preparazione e pubblicazione; render usa solo try_lock e restituisce silenzio durante una mutazione, senza attendere allocazioni. Una modifica può produrre una breve interruzione; nessun cambiamento alle formule DSP.
- slotType inizializzato a zero anche per gli slot vuoti. Rimossa SPECTRAL_ALLOW_PROCESS_ALLOCATION dalla compilazione: i test del processore ora eseguono lo stesso motore del plugin. Solo i render DSP diretti usano l'adattatore TestEngine.
- Nuovo stress del processore: 240 aggiunte tramite il vero ComboBox, rimozioni, riordino, salvataggio/richiamo, cambi sample rate, reset, resize e snapshot dell'editor mentre un secondo thread elabora audio. Verificati campioni finiti e recupero dell'audio.
- Release: 64 PASS. Stessa suite con AddressSanitizer e UndefinedBehaviorSanitizer: 64 PASS, nessun errore (leak detection disabilitata). Build AU/VST3 Release completata; solo warning Xcode sull'ordine manuale dei target.
- Stress AU reale corretto: 2.000 cambi di stato/catena, 8.627.104 letture concorrenti, guardie del buffer intatte; lista finale 66 parametri per 12 moduli. Script di test compilabile con clang++ -std=c++17 Tests/AUParameterStress.mm -framework Cocoa -framework AudioToolbox. Passare il percorso assoluto del binario AU.
- AU e VST3 0.7.15 installati nelle cartelle utente; backup in Backups/pre-0.7.15-20261023. auval conferma Component Version 0.7.15 e AU VALIDATION SUCCEEDED. Log in Builds/Bugfix20261023. Sessione reale di Logic non pilotata in questa verifica.
- Installer precedente: bundle di sistema root:wheel con mode 0700, illeggibili agli utenti ordinari. Build Installer.command imposta directory 0755, file 0644 e binari 0755, disabilita la rilocazione dei bundle e controlla che AU/VST3 abbiano la stessa versione. Nuovo pkg 0.7.15; firma locale, non notarizzato. Build attuale arm64 con deployment effettivo 15.7.

- Verifica finale ampliata a due lettori simultanei: PASS AU concurrent state/list queries: 27574 reads, 2000 chain changes, final parameters 66. Abilitato esplicitamente il mutex non realtime del SDK AU (di default nullo), per serializzare anche richieste host concorrenti. Verificata applicazione della patch sia da JUCE originale sia dalla precedente patch. Vecchio installer 0.7.13 conservato.


## Smear e tilt timbrico — 0.7.16 (23 ottobre 2026)

- SMEAR: due inviluppi di ampiezza in cascata, 1,5 s massimi per stadio; raccordo del controllo a 35 ms. La fase di sintesi non si riallinea a ogni nuovo attacco. Diffusione deterministica delle componenti rumorose per limitare la periodicità FFT; protezione delle note tonali mantenuta anche durante il decadimento. Il suono con SMEAR non nullo cambia intenzionalmente.
- Confronto 0.7.15/0.7.16 a 48 kHz, FFT 2048: transienti broadband ripetuti, SMEAR 1, picco 0,0614741 → 0,0260620 (−7,45 dB); transienti limitati in banda, picco 0,0280772 → 0,00851702 (−10,36 dB). SMEAR 0,5: 0,0768936 → 0,05336 e 0,036355 → 0,0294206. Sono misure su segnali sintetici, non una garanzia su ogni materiale musicale. Rinforzati i test di attacchi ripetuti a metà e a fondo corsa.
- Test coda 440 Hz: frazione di energia all'intonazione originale >0,9 su FFT 512/1024/2048/4096/8192. Verificati silenzio dopo reset, nessun leakage stereo, neutralità/bypass e Freeze già catturato invariato quando si muove SMEAR.
- Il TILT visibile controlla ora nuovi parametri toneTilt (−6…+6 dB/ottava, default 0, pivot 1 kHz, estremi limitati a ±18 dB). È indipendente dall'attivazione della soppressione del Gate e agisce soltanto nella banda selezionata. Pulsante rinominato GATE; tooltip e doppio clic a zero. I vecchi gateTilt mantengono ID, range, indice e funzione di soglia per non reinterpretare le automazioni. Nuovi parametri accodati con version hint 2, ripristino a zero nelle sessioni precedenti.
- Nuovi test tramite processore/parametri reali: 250/1000/4000 Hz, tilt ±3, FFT 512/2048/8192; risposta attesa rispettata (errore massimo misurato circa 0,12 dB). Verificati Gate spento, banda esclusa, canale silenzioso, salvataggio/richiamo e caricamento stato privo dei nuovi parametri.
- 67 PASS Release e 67 PASS AddressSanitizer/UndefinedBehaviorSanitizer, nessun errore (leak detection disabilitata). AU stress: 2.000 cambi di catena, 33.011 letture concorrenti, guardie intatte e 68 parametri finali per 12 moduli. auval 0.7.16: AU VALIDATION SUCCEEDED.
- Benchmark dopo le build, FFT 2048: tre effetti 2,06 → 2,45% DSP/audio; sei effetti 4,43 → 4,84%; otto riverberi 12,86 → 12,84%. Maggior costo limitato allo Smear rivisto. Log in Builds/SmearTilt20261023.
- AU/VST3 0.7.16 installati nelle cartelle utente. Backup della 0.7.15 in Backups/pre-0.7.16-20261023. Installer 0.7.16 creato, conservati i pkg 0.7.13 e 0.7.15. Nessun ascolto soggettivo in Logic dichiarato: valutazione musicale finale da fare sull'audio dell'utente.


## Interfaccia — 0.7.17 / 0.7.18

0.7.17: SMEAR nascosto solo nella grafica; DSP, valori e automazioni conservati.
0.7.18: TILT visualizza una sola unità dB/oct. La funzione di formato restituisce
solo il numero; Slider aggiunge già il suffisso. Modifiche esclusivamente grafiche.

## 0.7.19 — interfaccia in stile atelier (24 ottobre 2026)

- Build Xcode Release AU/VST3/test riuscita; 67 PASS. Nuovo pkg `Spectral Strip 0.7.19.pkg` (firma locale, non notarizzato).
- AU e VST3 0.7.19 installati nelle cartelle utente; backup in Backups/pre-atelier-ui-install-20261024-003241. auval: Component Version 0.7.19, AU VALIDATION SUCCEEDED. Log in Builds/AtelierUI20261024.
- In /Library/Audio/Plug-Ins resta una copia di sistema del 16 ottobre (root, mode 0700), non modificata.

## 0.7.20 — illustrazioni dei moduli legate ai parametri (24 ottobre 2026)

- Build Xcode Release AU/VST3/test riuscita; 67 PASS. Nuovo pkg `Spectral Strip 0.7.20.pkg`; il pkg 0.7.19 è conservato.
- AU e VST3 0.7.20 installati nelle cartelle utente; backup in Backups/pre-0.7.20-install-20261024-004847. auval: Component Version 0.7.20, AU VALIDATION SUCCEEDED. Log in Builds/ResponsiveArt20261024.

## 0.7.21 — menu "+" accanto allo slot (24 ottobre 2026)

- Il "+" dello slot tratteggiato apre l'elenco dei moduli subito sotto di sé, scalato con l'editor.
- Build Release riuscita; 67 PASS; nuovo pkg `Spectral Strip 0.7.21.pkg` (pkg precedenti conservati). AU/VST3 0.7.21 installati nelle cartelle utente, auval AU VALIDATION SUCCEEDED. Log in Builds/AddMenu20261024.

## 0.7.22 — scorrimento continuo della strip (24 ottobre 2026)

- I moduli scorrono in modo continuo: trackpad 1:1 con l'inerzia di macOS, rotella del mouse animata, barra di scorrimento e auto-scroll durante il trascinamento senza scatti di modulo. Gli slider dei moduli non reagiscono più alla rotella, che scorre la strip.
- Build Release riuscita; 67 PASS; nuovo pkg `Spectral Strip 0.7.22.pkg` (pkg precedenti conservati). AU/VST3 0.7.22 installati nelle cartelle utente, auval AU VALIDATION SUCCEEDED. Log in Builds/SmoothScroll20261024.
