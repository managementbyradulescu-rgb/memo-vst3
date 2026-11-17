# Memo – VST3 pentru mix rapid de voce

Plugin VST3 (Windows + macOS) cu lant complet de voce si o casuta unde scrii cum vrei sa sune
("vocal curat, cald, cu aer si putin delay") – Memo seteaza butoanele, iar tu poti rafina manual.

## Lantul (in ordine)
Noise suppressor -> Upward compression -> Peak tamer -> Resonance EQ -> Low cut (60-200 Hz, implicit 100)
-> Boxy cut -> Clarity + Air -> Saturatie -> De-esser -> FET 1176 -> Opto LA-2A -> Delay -> Reverb -> Boost volum (+ limitator soft de siguranta)

Threshold-urile sunt relative la nivelul mediu al vocii, deci merge pe orice nivel de inregistrare.

## Cum obtii fisierul .vst3 (fara sa instalezi nimic)
1. Creeaza un repo nou pe GitHub si urca tot continutul acestui folder (inclusiv `.github`).
2. Tab-ul **Actions** -> workflow-ul "Build VST3" porneste singur (sau "Run workflow"). Dureaza ~10-15 min.
3. Cand e verde, intra pe run -> **Artifacts** -> descarca `Memo-VST3-Windows` sau `Memo-VST3-macOS`.

## Instalare
- **Windows:** copiaza `Memo.vst3` in `C:\Program Files\Common Files\VST3\`
- **macOS:** copiaza in `/Library/Audio/Plug-Ins/VST3/`, apoi in Terminal: `xattr -cr /Library/Audio/Plug-Ins/VST3/Memo.vst3`
- Deschide DAW-ul si fa rescan de pluginuri. In FL Studio: Options > Manage plugins > Find plugins.

## Cuvinte cheie intelese (RO + EN)
curat/clean, cald/warm, aer/air, sibilante/harsh, zgomot/noise, mai tare/loud, incet/quiet, compress/uniform,
agresiv/trap/drill, soft/smooth, boxy/nazal, subtire/thin, natural, delay/echo/slap, reverb/hall/spatios, uscat/dry.
Modificatori: "foarte", "mult", "extra" (mai mult) si "putin", "usor", "un pic" (mai putin).
