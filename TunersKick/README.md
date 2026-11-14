# Tuners Kick (VST3 do FL Studio)

Syntezator kicka pod hard techno i hardstyle: tryby Barrel, Industrial, Metal, Rawstyle, własny distortion/clipper z oversamplingiem 4x. Wszystkie parametry da się automatyzować w FL Studio. Presety wybierasz jako programy wtyczki (menu presetów w oknie wtyczki).

## Budowanie (Windows)

Potrzebujesz:
1. Visual Studio Community 2022 (lub nowsze) z pakietem **Desktop development with C++**
2. Git (jest w instalatorze Visual Studio jako komponent, albo z git-scm.com)

W folderze projektu otwórz **Developer PowerShell for VS** i wpisz:

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target TunersKick_VST3
```

Pierwsza konfiguracja pobiera JUCE z GitHuba (kilka minut). Jeśli masz nowszy Visual Studio, zmień nazwę generatora (`cmake --help` wypisuje dostępne).

Gotowy plik: `build\TunersKick_artefacts\Release\VST3\Tuners Kick.vst3` (to folder, kopiuj cały).

## Instalacja w FL Studio

1. Skopiuj `Tuners Kick.vst3` do `C:\Program Files\Common Files\VST3\` (potrzebne uprawnienia administratora).
2. FL Studio: **Options → Manage plugins → Find installed plugins** (Start scan).
3. Dodaj wtyczkę z listy instrumentów i zagraj dowolną nutą. Każda nuta wyzwala kick. Z włączonym **Key Track** wysokość idzie za nutą (C2 = bez zmiany).

## Parametry

- **Body:** Tune (nuta bazowa, np. F1), Pitch Amount i Pitch Decay (długość i zakres opadania), Length, Body Drive
- **Click:** poziom, częstotliwość i długość kliknięcia na starcie
- **Barrel:** rezonator dający pusty, beczkowy ton. Freq, Q, Track (jak mocno podąża za pitch envelope)
- **Metal:** FM z nieharmonicznym stosunkiem (Ratio) i dzwoniącym ogonem
- **Rumble:** szum w tonacji kicka, narasta i wolno opada
- **Distortion / Output:** Soft, Hard lub Fold, Drive, Dist Mix, Clip (ścina i dogłaśnia sygnał), Output (maks. 0 dB, więc wyjście nie przekroczy 0 dBFS), Vel Sens

## Uwagi

- Presety to punkt startowy, brzmienie dostroisz uchem.
- Okno wtyczki jest na razie generyczne (suwaki z JUCE). Własne GUI to kolejny etap.
- JUCE ma własną licencję (Personal / komercyjna / AGPL). Przed publikacją wtyczki sprawdź warunki na juce.com.
